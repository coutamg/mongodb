/**
 *    Copyright (C) 2016 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/db/s/balancer/balancer_chunk_selection_policy_impl.h"

#include <set>
#include <vector>

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonobj_comparator_interface.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog/type_chunk.h"
#include "mongo/s/catalog/type_collection.h"
#include "mongo/s/catalog/type_tags.h"
#include "mongo/s/catalog_cache.h"
#include "mongo/s/grid.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

//BalancerChunkSelectionPolicyImpl::selectChunksToMove????
using MigrateInfoVector = BalancerChunkSelectionPolicy::MigrateInfoVector;
using SplitInfoVector = BalancerChunkSelectionPolicy::SplitInfoVector;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

namespace {

/**
 * Does a linear pass over the information cached in the specified chunk manager and extracts chunk
 * distribution and chunk placement information which is needed by the balancer policy.
 */
StatusWith<DistributionStatus> createCollectionDistributionStatus(
    OperationContext* opCtx, const ShardStatisticsVector& allShards, ChunkManager* chunkMgr) {
    ShardToChunksMap shardToChunksMap;

    // Makes sure there is an entry in shardToChunksMap for every shard, so empty shards will also
    // be accounted for
    for (const auto& stat : allShards) {
        shardToChunksMap[stat.shardId];
    }

    for (const auto& chunkEntry : chunkMgr->chunks()) {
        ChunkType chunk;
        chunk.setNS(chunkMgr->getns());
        chunk.setMin(chunkEntry->getMin());
        chunk.setMax(chunkEntry->getMax());
        chunk.setJumbo(chunkEntry->isJumbo());
        chunk.setShard(chunkEntry->getShardId());
        chunk.setVersion(chunkEntry->getLastmod());

        shardToChunksMap[chunkEntry->getShardId()].push_back(chunk);
    }

    vector<TagsType> collectionTags;
    Status tagsStatus = Grid::get(opCtx)->catalogClient()->getTagsForCollection(
        opCtx, chunkMgr->getns(), &collectionTags);
    if (!tagsStatus.isOK()) {
        return {tagsStatus.code(),
                str::stream() << "Unable to load tags for collection " << chunkMgr->getns()
                              << " due to "
                              << tagsStatus.toString()};
    }

    DistributionStatus distribution(NamespaceString(chunkMgr->getns()),
                                    std::move(shardToChunksMap));

    // Cache the collection tags
    const auto& keyPattern = chunkMgr->getShardKeyPattern().getKeyPattern();

    for (const auto& tag : collectionTags) {
        auto status = distribution.addRangeToZone(
            ZoneRange(keyPattern.extendRangeBound(tag.getMinKey(), false),
                      keyPattern.extendRangeBound(tag.getMaxKey(), false),
                      tag.getTag()));

        if (!status.isOK()) {
            return status;
        }
    }

    return {std::move(distribution)};
}

/**
 * Helper class used to accumulate the split points for the same chunk together so they can be
 * submitted to the shard as a single call versus multiple. This is necessary in order to avoid
 * refreshing the chunk metadata after every single split point (if done one by one), because
 * splitting a chunk does not yield the same chunk anymore.
 */
class SplitCandidatesBuffer {
    MONGO_DISALLOW_COPYING(SplitCandidatesBuffer);

public:
    SplitCandidatesBuffer(NamespaceString nss, ChunkVersion collectionVersion)
        : _nss(std::move(nss)),
          _collectionVersion(collectionVersion),
          _chunkSplitPoints(SimpleBSONObjComparator::kInstance
                                .makeBSONObjIndexedMap<BalancerChunkSelectionPolicy::SplitInfo>()) {
    }

    /**
     * Adds the specified split point to the chunk. The split points must always be within the
     * boundaries of the chunk and must come in increasing order.
     */
    void addSplitPoint(shared_ptr<Chunk> chunk, const BSONObj& splitPoint) {
        auto it = _chunkSplitPoints.find(chunk->getMin());
        if (it == _chunkSplitPoints.end()) {
            _chunkSplitPoints.emplace(chunk->getMin(),
                                      BalancerChunkSelectionPolicy::SplitInfo(chunk->getShardId(),
                                                                              _nss,
                                                                              _collectionVersion,
                                                                              chunk->getLastmod(),
                                                                              chunk->getMin(),
                                                                              chunk->getMax(),
                                                                              {splitPoint}));
        } else if (splitPoint.woCompare(it->second.splitKeys.back()) > 0) {
            it->second.splitKeys.push_back(splitPoint);
        } else {
            // Split points must come in order
            invariant(splitPoint.woCompare(it->second.splitKeys.back()) == 0);
        }
    }

    /**
     * May be called only once for the lifetime of the buffer. Moves the contents of the buffer into
     * a vector of split infos to be passed to the split call.
     */
    SplitInfoVector done() {
        BalancerChunkSelectionPolicy::SplitInfoVector splitPoints;
        for (const auto& entry : _chunkSplitPoints) {
            splitPoints.push_back(std::move(entry.second));
        }

        return splitPoints;
    }

private:
    // Namespace and expected collection version
    const NamespaceString _nss;
    const ChunkVersion _collectionVersion;

    // Chunk min key and split vector associated with that chunk
    BSONObjIndexedMap<BalancerChunkSelectionPolicy::SplitInfo> _chunkSplitPoints;
};

}  // namespace

BalancerChunkSelectionPolicyImpl::BalancerChunkSelectionPolicyImpl(ClusterStatistics* clusterStats)
    : _clusterStats(clusterStats) {}

BalancerChunkSelectionPolicyImpl::~BalancerChunkSelectionPolicyImpl() = default;

StatusWith<SplitInfoVector> BalancerChunkSelectionPolicyImpl::selectChunksToSplit(
    OperationContext* opCtx) {
    auto shardStatsStatus = _clusterStats->getStats(opCtx);
    if (!shardStatsStatus.isOK()) {
        return shardStatsStatus.getStatus();
    }

    const auto shardStats = std::move(shardStatsStatus.getValue());

    vector<CollectionType> collections;

    Status collsStatus =
        Grid::get(opCtx)->catalogClient()->getCollections(opCtx, nullptr, &collections, nullptr);
    if (!collsStatus.isOK()) {
        return collsStatus;
    }

    if (collections.empty()) {
        return SplitInfoVector{};
    }

    SplitInfoVector splitCandidates;

    for (const auto& coll : collections) {
        if (coll.getDropped()) {
            continue;
        }

        const NamespaceString nss(coll.getNs());

        auto candidatesStatus = _getSplitCandidatesForCollection(opCtx, nss, shardStats);
        if (candidatesStatus == ErrorCodes::NamespaceNotFound) {
            // Namespace got dropped before we managed to get to it, so just skip it
            continue;
        } else if (!candidatesStatus.isOK()) {
            warning() << "Unable to enforce tag range policy for collection " << nss.ns()
                      << causedBy(candidatesStatus.getStatus());
            continue;
        }

        splitCandidates.insert(splitCandidates.end(),
                               std::make_move_iterator(candidatesStatus.getValue().begin()),
                               std::make_move_iterator(candidatesStatus.getValue().end()));
    }

    return splitCandidates;
}

/*
??????????chunks????:
xx_HTZjQeZL_shard_1 571274
xx_HTZjQeZL_shard_2 319536
xx_HTZjQeZL_shard_3 572644
xx_HTZjQeZL_shard_4 707811
xx_HTZjQeZL_shard_QubQcjup	145339
xx_HTZjQeZL_shard_ewMvmPnE	136034
xx_HTZjQeZL_shard_jaAVvOei	129682
xx_HTZjQeZL_shard_kxYilhNF	150150

??????????????????????chunk
mongos> db.migrations.find()
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"359209050\"module_\"album\"md5_\"992F3FF0DDCB009D1A6CCD8647CEAFA5\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "359209050", "module" : "album", "md5" : "992F3FF0DDCB009D1A6CCD8647CEAFA5" }, "max" : { "xx" : "359209058", "module" : "album", "md5" : "49D552BEBEAE7D3CB6B53A7FE384E5A4" }, "fromShard" : "xx_HTZjQeZL_shard_1", "toShard" : "xx_HTZjQeZL_shard_QubQcjup", "chunkVersion" : [ Timestamp(588213, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"278344065\"module_\"album\"md5_\"CAA4B99617A83D0DEE5CE30D4D75829F\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "278344065", "module" : "album", "md5" : "CAA4B99617A83D0DEE5CE30D4D75829F" }, "max" : { "xx" : "278344356", "module" : "album", "md5" : "E691E5F8756E723BB44BC2049D624F81" }, "fromShard" : "xx_HTZjQeZL_shard_4", "toShard" : "xx_HTZjQeZL_shard_jaAVvOei", "chunkVersion" : [ Timestamp(588211, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"226060685\"module_\"album\"md5_\"56A0293EAD54C7A1326C91621A7C4664\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "226060685", "module" : "album", "md5" : "56A0293EAD54C7A1326C91621A7C4664" }, "max" : { "xx" : "226061085", "module" : "album", "md5" : "6686C24B301C39C3B3585D8602A26F45" }, "fromShard" : "xx_HTZjQeZL_shard_3", "toShard" : "xx_HTZjQeZL_shard_ewMvmPnE", "chunkVersion" : [ Timestamp(588212, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
*/

//??????????????chunk,????MigrateInfoVector??????chunk????  
//Balancer::_mainThread() balancer????????????
StatusWith<MigrateInfoVector> BalancerChunkSelectionPolicyImpl::selectChunksToMove(
    OperationContext* opCtx, bool aggressiveBalanceHint) {
    //ClusterStatisticsImpl::getStats ??????????????shardStatsStatus????????????????????????????????????
    auto shardStatsStatus = _clusterStats->getStats(opCtx);
    if (!shardStatsStatus.isOK()) {
        return shardStatsStatus.getStatus();
    }
	
    const auto shardStats = std::move(shardStatsStatus.getValue());

	//??????????1????????????????????????????????????2??????
    if (shardStats.size() < 2) {
        return MigrateInfoVector{};
    }

    vector<CollectionType> collections;

	//????????????????
    Status collsStatus =
        Grid::get(opCtx)->catalogClient()->getCollections(opCtx, nullptr, &collections, nullptr);
    if (!collsStatus.isOK()) {
        return collsStatus;
    }

	//????????????????????
    if (collections.empty()) {
        return MigrateInfoVector{};
    }

    MigrateInfoVector candidateChunks;

    for (const auto& coll : collections) {
        if (coll.getDropped()) {
            continue;
        }

        const NamespaceString nss(coll.getNs());

		//sh.disableBalancing(??db.collection??) ????????balance????
        if (!coll.getAllowBalance()) {
            LOG(1) << "Not balancing collection " << nss << "; explicitly disabled.";
            continue;
        }

		//??????????????chunk
        auto candidatesStatus =
            _getMigrateCandidatesForCollection(opCtx, nss, shardStats, aggressiveBalanceHint);
        if (candidatesStatus == ErrorCodes::NamespaceNotFound) {
            // Namespace got dropped before we managed to get to it, so just skip it
            continue;
        } else if (!candidatesStatus.isOK()) {
            warning() << "Unable to balance collection " << nss.ns()
                      << causedBy(candidatesStatus.getStatus());
            continue;
        }

        candidateChunks.insert(candidateChunks.end(),
                               std::make_move_iterator(candidatesStatus.getValue().begin()),
                               std::make_move_iterator(candidatesStatus.getValue().end()));
    }

	//??????????????chunk  MigrateInfoVector????
    return candidateChunks;
}

StatusWith<boost::optional<MigrateInfo>>
BalancerChunkSelectionPolicyImpl::selectSpecificChunkToMove(OperationContext* opCtx,
                                                            const ChunkType& chunk) {
    auto shardStatsStatus = _clusterStats->getStats(opCtx);
    if (!shardStatsStatus.isOK()) {
        return shardStatsStatus.getStatus();
    }

    const auto& shardStats = shardStatsStatus.getValue();

    auto routingInfoStatus =
        Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx,
                                                                                     chunk.getNS());
    if (!routingInfoStatus.isOK()) {
        return routingInfoStatus.getStatus();
    }

    const auto cm = routingInfoStatus.getValue().cm().get();

    const auto collInfoStatus = createCollectionDistributionStatus(opCtx, shardStats, cm);
    if (!collInfoStatus.isOK()) {
        return collInfoStatus.getStatus();
    }

    const DistributionStatus& distribution = collInfoStatus.getValue();

    return BalancerPolicy::balanceSingleChunk(chunk, shardStats, distribution);
}

Status BalancerChunkSelectionPolicyImpl::checkMoveAllowed(OperationContext* opCtx,
                                                          const ChunkType& chunk,
                                                          const ShardId& newShardId) {
    auto shardStatsStatus = _clusterStats->getStats(opCtx);
    if (!shardStatsStatus.isOK()) {
        return shardStatsStatus.getStatus();
    }

    auto shardStats = std::move(shardStatsStatus.getValue());

    auto routingInfoStatus =
        Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx,
                                                                                     chunk.getNS());
    if (!routingInfoStatus.isOK()) {
        return routingInfoStatus.getStatus();
    }

    const auto cm = routingInfoStatus.getValue().cm().get();

    const auto collInfoStatus = createCollectionDistributionStatus(opCtx, shardStats, cm);
    if (!collInfoStatus.isOK()) {
        return collInfoStatus.getStatus();
    }

    const DistributionStatus& distribution = collInfoStatus.getValue();

    auto newShardIterator =
        std::find_if(shardStats.begin(),
                     shardStats.end(),
                     [&newShardId](const ClusterStatistics::ShardStatistics& stat) {
                         return stat.shardId == newShardId;
                     });
    if (newShardIterator == shardStats.end()) {
        return {ErrorCodes::ShardNotFound,
                str::stream() << "Unable to find constraints information for shard " << newShardId
                              << ". Move to this shard will be disallowed."};
    }

    return BalancerPolicy::isShardSuitableReceiver(*newShardIterator,
                                                   distribution.getTagForChunk(chunk));
}

StatusWith<SplitInfoVector> BalancerChunkSelectionPolicyImpl::_getSplitCandidatesForCollection(
    OperationContext* opCtx, const NamespaceString& nss, const ShardStatisticsVector& shardStats) {
    auto routingInfoStatus =
        Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx, nss);
    if (!routingInfoStatus.isOK()) {
        return routingInfoStatus.getStatus();
    }

    const auto cm = routingInfoStatus.getValue().cm().get();

    const auto& shardKeyPattern = cm->getShardKeyPattern().getKeyPattern();

    const auto collInfoStatus = createCollectionDistributionStatus(opCtx, shardStats, cm);
    if (!collInfoStatus.isOK()) {
        return collInfoStatus.getStatus();
    }

    const DistributionStatus& distribution = collInfoStatus.getValue();

    // Accumulate split points for the same chunk together
    SplitCandidatesBuffer splitCandidates(nss, cm->getVersion());

    for (const auto& tagRangeEntry : distribution.tagRanges()) {
        const auto& tagRange = tagRangeEntry.second;

        shared_ptr<Chunk> chunkAtZoneMin =
            cm->findIntersectingChunkWithSimpleCollation(tagRange.min);
        invariant(chunkAtZoneMin->getMax().woCompare(tagRange.min) > 0);

        if (chunkAtZoneMin->getMin().woCompare(tagRange.min)) {
            splitCandidates.addSplitPoint(chunkAtZoneMin, tagRange.min);
        }

        // The global max key can never fall in the middle of a chunk
        if (!tagRange.max.woCompare(shardKeyPattern.globalMax()))
            continue;

        shared_ptr<Chunk> chunkAtZoneMax =
            cm->findIntersectingChunkWithSimpleCollation(tagRange.max);

        // We need to check that both the chunk's minKey does not match the zone's max and also that
        // the max is not equal, which would only happen in the case of the zone ending in MaxKey.
        if (chunkAtZoneMax->getMin().woCompare(tagRange.max) &&
            chunkAtZoneMax->getMax().woCompare(tagRange.max)) {
            splitCandidates.addSplitPoint(chunkAtZoneMax, tagRange.max);
        }
    }

    return splitCandidates.done();
}

//BalancerChunkSelectionPolicyImpl::selectChunksToMove????
StatusWith<MigrateInfoVector> BalancerChunkSelectionPolicyImpl::_getMigrateCandidatesForCollection(
    OperationContext* opCtx,
    const NamespaceString& nss,
    const ShardStatisticsVector& shardStats,
    bool aggressiveBalanceHint) {
    auto routingInfoStatus =
        Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx, nss);
    if (!routingInfoStatus.isOK()) {
        return routingInfoStatus.getStatus();
    }

    const auto cm = routingInfoStatus.getValue().cm().get();

    const auto& shardKeyPattern = cm->getShardKeyPattern().getKeyPattern();

    const auto collInfoStatus = createCollectionDistributionStatus(opCtx, shardStats, cm);
    if (!collInfoStatus.isOK()) {
        return collInfoStatus.getStatus();
    }

    const DistributionStatus& distribution = collInfoStatus.getValue();

    for (const auto& tagRangeEntry : distribution.tagRanges()) {
        const auto& tagRange = tagRangeEntry.second;

        shared_ptr<Chunk> chunkAtZoneMin =
            cm->findIntersectingChunkWithSimpleCollation(tagRange.min);

        if (chunkAtZoneMin->getMin().woCompare(tagRange.min)) {
            return {ErrorCodes::IllegalOperation,
                    str::stream()
                        << "Tag boundaries "
                        << tagRange.toString()
                        << " fall in the middle of an existing chunk "
                        << ChunkRange(chunkAtZoneMin->getMin(), chunkAtZoneMin->getMax()).toString()
                        << ". Balancing for collection "
                        << nss.ns()
                        << " will be postponed until the chunk is split appropriately."};
        }

        // The global max key can never fall in the middle of a chunk
        if (!tagRange.max.woCompare(shardKeyPattern.globalMax()))
            continue;

        shared_ptr<Chunk> chunkAtZoneMax =
            cm->findIntersectingChunkWithSimpleCollation(tagRange.max);

        // We need to check that both the chunk's minKey does not match the zone's max and also that
        // the max is not equal, which would only happen in the case of the zone ending in MaxKey.
        if (chunkAtZoneMax->getMin().woCompare(tagRange.max) &&
            chunkAtZoneMax->getMax().woCompare(tagRange.max)) {
            return {ErrorCodes::IllegalOperation,
                    str::stream()
                        << "Tag boundaries "
                        << tagRange.toString()
                        << " fall in the middle of an existing chunk "
                        << ChunkRange(chunkAtZoneMax->getMin(), chunkAtZoneMax->getMax()).toString()
                        << ". Balancing for collection "
                        << nss.ns()
                        << " will be postponed until the chunk is split appropriately."};
        }
    }

    return BalancerPolicy::balance(shardStats, distribution, aggressiveBalanceHint);
}

}  // namespace mongo
