/**
 *    Copyright (C) 2015 MongoDB Inc.
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

#include "mongo/s/catalog_cache.h"

#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/db/query/collation/collator_factory_interface.h"
#include "mongo/db/repl/optime_with.h"
#include "mongo/platform/unordered_set.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog/type_collection.h"
#include "mongo/s/catalog/type_database.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/util/concurrency/with_lock.h"
#include "mongo/util/log.h"
#include "mongo/util/timer.h"

namespace mongo {
namespace {

// How many times to try refreshing the routing info if the set of chunks loaded from the config
// server is found to be inconsistent.
const int kMaxInconsistentRoutingInfoRefreshAttempts = 3;

/**
 * Given an (optional) initial routing table and a set of changed chunks returned by the catalog
 * cache loader, produces a new routing table with the changes applied.
 *
 * If the collection is no longer sharded returns nullptr. If the epoch has changed, expects that
 * the 'collectionChunksList' contains the full contents of the chunks collection for that namespace
 * so that the routing table can be built from scratch.
 *
 * Throws ConflictingOperationInProgress if the chunk metadata was found to be inconsistent (not
 * containing all the necessary chunks, contains overlaps or chunks' epoch values are not the same
 * as that of the collection). Since this situation may be transient, due to the collection being
 * dropped or recreated concurrently, the caller must retry the reload up to some configurable
 * number of attempts.
 */ 

//????????chunk????swCollectionAndChangedChunks??nss??????existingRoutingInfo
//????nss??????ChunkManager   

//CatalogCache::_scheduleCollectionRefresh????
std::shared_ptr<ChunkManager> 
 refreshCollectionRoutingInfo(
    OperationContext* opCtx,
    const NamespaceString& nss,
    //????????chunk????swCollectionAndChangedChunks??nss??????existingRoutingInfo
    std::shared_ptr<ChunkManager> existingRoutingInfo,
    //swCollectionAndChangedChunks????????ConfigServerCatalogCacheLoader::getChunksSince
    StatusWith<CatalogCacheLoader::CollectionAndChangedChunks> swCollectionAndChangedChunks) {
    if (swCollectionAndChangedChunks == ErrorCodes::NamespaceNotFound) {
        return nullptr;
    }

	//????ConfigServerCatalogCacheLoader::getChunksSince????swCollAndChunks
	//cfg??config.chunks??????????????????????lastmod????????????????chunk
    const auto collectionAndChunks = uassertStatusOK(std::move(swCollectionAndChangedChunks));

	
    auto chunkManager = [&] {
        // If we have routing info already and it's for the same collection epoch, we're updating.
        // Otherwise, we're making a whole new routing table.
        //????CatalogCache::_scheduleCollectionRefresh->ConfigServerCatalogCacheLoader::getChunksSince  
        //????????????????????????????????????????????????
        //??????????
        if (existingRoutingInfo &&
            existingRoutingInfo->getVersion().epoch() == collectionAndChunks.epoch) {
			 //????????chunk????swCollectionAndChangedChunks??nss??????existingRoutingInfo
			 //ChunkManager::makeUpdated,collectionAndChunks.changedChunks??????????chunks????????????cfg????????????????????

			 //ChunkManager::makeUpdated
			 return existingRoutingInfo->makeUpdated(collectionAndChunks.changedChunks);
        }

		//??????epoll????????????????rename??????????????????????????ChunkManager??
        auto defaultCollator = [&]() -> std::unique_ptr<CollatorInterface> {
            if (!collectionAndChunks.defaultCollation.isEmpty()) {
                // The collation should have been validated upon collection creation
                return uassertStatusOK(CollatorFactoryInterface::get(opCtx->getServiceContext())
                                           ->makeFromBSON(collectionAndChunks.defaultCollation));
            }
            return nullptr;
        }();

		//????????ChunkManager??
        return ChunkManager::makeNew(nss,
                                     collectionAndChunks.uuid,
                                     KeyPattern(collectionAndChunks.shardKeyPattern),
                                     std::move(defaultCollator),
                                     collectionAndChunks.shardKeyIsUnique,
                                     collectionAndChunks.epoch,
                                     collectionAndChunks.changedChunks);
    }();

    std::set<ShardId> shardIds;
	//ChunkManager::getAllShardIds  //????????????chunk????????id????????shardIds
    chunkManager->getAllShardIds(&shardIds);
    for (const auto& shardId : shardIds) {
		//????shardId??????????Shard????  Grid::shardRegistry->ShardRegistry::getShard
        uassertStatusOK(Grid::get(opCtx)->shardRegistry()->getShard(opCtx, shardId));
    }
    return chunkManager;
}

}  // namespace

CatalogCache::CatalogCache(CatalogCacheLoader& cacheLoader) : _cacheLoader(cacheLoader) {}

CatalogCache::~CatalogCache() = default;

//??cfg????dbName????????????
StatusWith<CachedDatabaseInfo> CatalogCache::getDatabase(OperationContext* opCtx,
                                                         StringData dbName) {
    try {
        return {CachedDatabaseInfo(_getDatabase(opCtx, dbName))};
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

//????????https://mongoing.com/archives/75945
//https://mongoing.com/archives/77370

//(??????????????????????chunks??????????metadata??????????????????)?????? 
//    ShardingState::_refreshMetadata->CatalogCache::getShardedCollectionRoutingInfoWithRefresh

//????????????????:
//moveChunk  checkShardVersion   ChunkSplitter::_runAutosplit splitIfNeeded  
//DropCmd::run RenameCollectionCmd  ClusterFind::runQuery  FindAndModifyCmd::run  CollectionStats::run
//ClusterAggregate::runAggregate    getExecutionNsRoutingInfo  dispatchShardPipeline
//ChunkManagerTargeter::init??????????????????????????
StatusWith<CachedCollectionRoutingInfo> 
//CatalogCache::onStaleConfigError??CatalogCache::invalidateShardedCollection??needsRefresh??????true
//????????????cfg????????????????
  CatalogCache::getCollectionRoutingInfo(//????????????needsRefresh??true????????????????
    OperationContext* opCtx, const NamespaceString& nss) {
    while (true) {
        std::shared_ptr<DatabaseInfoEntry> dbEntry;
        try {
			//??cfg????????config.database??config.collections??????dbName??????????????????
            dbEntry = _getDatabase(opCtx, nss.db());
        } catch (const DBException& ex) {
            return ex.toStatus();
        }

		//??????????????
        stdx::unique_lock<stdx::mutex> ul(_mutex);

        auto& collections = dbEntry->collections;

		//????db??????????????????nss??
        auto it = collections.find(nss.ns());
		//??nss????DB????????????nss
        if (it == collections.end()) { 
            auto shardStatus =
				//????primaryShardId??????????????shard????   ShardRegistry::getShard????Shard????
				//??????????????????????shard????
                Grid::get(opCtx)->shardRegistry()->getShard(opCtx, dbEntry->primaryShardId);
            if (!shardStatus.isOK()) {
                return {ErrorCodes::Error(40371),
                        str::stream() << "The primary shard for collection " << nss.ns()
                                      << " could not be loaded due to error "
                                      << shardStatus.getStatus().toString()};
            }

			//????CachedCollectionRoutingInfo????
            return {CachedCollectionRoutingInfo(
                dbEntry->primaryShardId, nss, std::move(shardStatus.getValue()))};
        }
		
		//????????????????????

		//????nss??????CollectionRoutingInfoEntry????????????????????chunk????
        auto& collEntry = it->second;

		//????????????needsRefresh??true????????????????
        if (collEntry.needsRefresh) { //????????????????????????????
            auto refreshNotification = collEntry.refreshCompletionNotification;
            if (!refreshNotification) {
                refreshNotification = (collEntry.refreshCompletionNotification =
                                           std::make_shared<Notification<Status>>());
				//????dbEntry??????????nss??????chunks????????
                _scheduleCollectionRefresh(ul, dbEntry, std::move(collEntry.routingInfo), nss, 1);
            }

            // Wait on the notification outside of the mutex
            ul.unlock();

            auto refreshStatus = [&]() {
                try {
                    return refreshNotification->get(opCtx);
                } catch (const DBException& ex) {
                    return ex.toStatus();
                }
            }();

            if (!refreshStatus.isOK()) {
                return refreshStatus;
            }

            // Once the refresh is complete, loop around to get the latest value
            continue;
        }

		//????????????????????chunk????????
        return {CachedCollectionRoutingInfo(dbEntry->primaryShardId, collEntry.routingInfo)};
    }
}

//ChunkManagerTargeter::init??????????????????
StatusWith<CachedCollectionRoutingInfo> CatalogCache::getCollectionRoutingInfo(
    OperationContext* opCtx, StringData ns) {
    return getCollectionRoutingInfo(opCtx, NamespaceString(ns));
}

StatusWith<CachedCollectionRoutingInfo> CatalogCache::getCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, const NamespaceString& nss) {
    //????needsRefresh??true??????getCollectionRoutingInfo??????????????
    invalidateShardedCollection(nss);
    return getCollectionRoutingInfo(opCtx, nss);
}


//updateChunkWriteStatsAndSplitIfNeeded   ShardingState::_refreshMetadata??????
//SessionsCollectionSharded::_checkCacheForSessionsCollection
//????????nss????????????????
StatusWith<CachedCollectionRoutingInfo> CatalogCache::getShardedCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, const NamespaceString& nss) {
    //????needsRefresh??true??????getCollectionRoutingInfo??????????????
    invalidateShardedCollection(nss);

    auto routingInfoStatus = getCollectionRoutingInfo(opCtx, nss);
    if (routingInfoStatus.isOK() && !routingInfoStatus.getValue().cm()) {
        return {ErrorCodes::NamespaceNotSharded,
                str::stream() << "Collection " << nss.ns() << " is not sharded."};
    }

    return routingInfoStatus;
}

StatusWith<CachedCollectionRoutingInfo> CatalogCache::getShardedCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, StringData ns) {
    return getShardedCollectionRoutingInfoWithRefresh(opCtx, NamespaceString(ns));
}

//CatalogCache::onStaleConfigError??CatalogCache::invalidateShardedCollection??needsRefresh??????true
//????????????cfg????????????????


//ClusterFind::runQuery  
//MoveChunkCmd::errmsgRun
//SplitCollectionCmd::errmsgRun
//ChunkManagerTargeter::refreshNow
//????ccriToInvalidate??????????????????????
void CatalogCache::onStaleConfigError(CachedCollectionRoutingInfo&& ccriToInvalidate) {
    // Ensure the move constructor of CachedCollectionRoutingInfo is invoked in order to clear the
    // input argument so it can't be used anymore
    auto ccri(ccriToInvalidate);

	//??CachedCollectionRoutingInfo????cm????????????????routeinfo????collections????????????chunk????cm????
    if (!ccri._cm) {
        // Here we received a stale config error for a collection which we previously thought was
        // unsharded.
        //????needsRefresh??true??????getCollectionRoutingInfo??????????????
        invalidateShardedCollection(ccri._nss);
        return;
    }

    // Here we received a stale config error for a collection which we previously though was sharded
    stdx::lock_guard<stdx::mutex> lg(_mutex);

	//????ccri??????DB??DB????????????collection??????????
    auto it = _databases.find(NamespaceString(ccri._cm->getns()).db());
    if (it == _databases.end()) {
        // If the database does not exist, the collection must have been dropped so there is
        // nothing to invalidate. The getCollectionRoutingInfo will handle the reload of the
        // entire database and its collections.
        return;
    }

	//????DB??????????collection
    auto& collections = it->second->collections;

	//ccri._cm chunk manager????????????????
    auto itColl = collections.find(ccri._cm->getns());
    if (itColl == collections.end()) {
        // If the collection does not exist, this means it must have been dropped since the last
        // time we retrieved a cache entry for it. Doing nothing in this case will cause the
        // next call to getCollectionRoutingInfo to return an unsharded collection.
        return;
    } else if (itColl->second.needsRefresh) {
        // Refresh has been scheduled for the collection already
        return;
    } else if (itColl->second.routingInfo->getVersion() == ccri._cm->getVersion()) {
    	//??????????????????????????
        // If the versions match, the last version of the routing information that we used is no
        // longer valid, so trigger a refresh.
        itColl->second.needsRefresh = true;
    }
}


//CatalogCache::onStaleConfigError??CatalogCache::invalidateShardedCollection??needsRefresh??????true
//????????????cfg????????????????

//updateChunkWriteStatsAndSplitIfNeeded CatalogCache::getShardedCollectionRoutingInfoWithRefresh????
//getCollectionRoutingInfoWithRefresh   CatalogCache::onStaleConfigError
//updateChunkWriteStatsAndSplitIfNeeded
//??????????????????collection????????????????????????????????collection????????????????????cfg????
//????????????
void CatalogCache::invalidateShardedCollection(const NamespaceString& nss) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);

    auto it = _databases.find(nss.db());
    if (it == _databases.end()) {
        return;
    }

    it->second->collections[nss.ns()].needsRefresh = true;
}

void CatalogCache::invalidateShardedCollection(StringData ns) {
    invalidateShardedCollection(NamespaceString(ns));
}

/**
 * Non-blocking method, which removes the entire specified database (including its collections)
 * from the cache.
 */
void CatalogCache::purgeDatabase(StringData dbName) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);
    _databases.erase(dbName);
}


/**
 * Non-blocking method, which removes all databases (including their collections) from the
 * cache.
 */
void CatalogCache::purgeAllDatabases() {
    stdx::lock_guard<stdx::mutex> lg(_mutex);
    _databases.clear();
}

//CatalogCache::getCollectionRoutingInfo  CatalogCache::getDatabase????
//??????cachez????????????cache????????cfg????????config.database??config.collections??????dbName??????????????????
std::shared_ptr<CatalogCache::DatabaseInfoEntry> CatalogCache::_getDatabase(OperationContext* opCtx,
                                                                            StringData dbName) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);

	//??????db????????????????????????????????
    auto it = _databases.find(dbName);
    if (it != _databases.end()) {
        return it->second;
    }

	//??????????????????cfg??????


	//Grid::catalogClient????ShardingCatalogClient   
    const auto catalogClient = Grid::get(opCtx)->catalogClient();

    const auto dbNameCopy = dbName.toString();

    // Load the database entry
    //sharding_catalog_client_impl::getDatabase??cfg??????config.database????????dbName????????DatabaseType????????
    const auto opTimeWithDb = uassertStatusOK(catalogClient->getDatabase(
        opCtx, dbNameCopy, repl::ReadConcernLevel::kMajorityReadConcern));
	//DatabaseType????
    const auto& dbDesc = opTimeWithDb.value;

    // Load the sharded collections entries
    std::vector<CollectionType> collections;
    repl::OpTime collLoadConfigOptime;
    uassertStatusOK(
		//sharding_catalog_client_impl::getCollections 
		//????config.collections??_id??????????????
        catalogClient->getCollections(opCtx, &dbNameCopy, &collections, &collLoadConfigOptime));

    StringMap<CollectionRoutingInfoEntry> collectionEntries;
    for (const auto& coll : collections) {
		//????????????
        if (coll.getDropped()) {
            continue;
        }

		//??????????????????????????????DB????????????????????DB????????collection????????
        collectionEntries[coll.getNs().ns()].needsRefresh = true;
    }

	//db????????????collections(config.collections)??????????????????_databases??
    return _databases[dbName] = std::shared_ptr<DatabaseInfoEntry>(new DatabaseInfoEntry{
               dbDesc.getPrimary(), dbDesc.getSharded(), std::move(collectionEntries)});
}

//????dbEntry??????????nss??????chunks???????? 
//CatalogCache::getCollectionRoutingInfo??????
void CatalogCache::_scheduleCollectionRefresh(WithLock lk,
                                              std::shared_ptr<DatabaseInfoEntry> dbEntry,
                                              std::shared_ptr<ChunkManager> existingRoutingInfo,
                                              NamespaceString const& nss,
                                              int refreshAttempt) {
    Timer t;

	//??????????ChunkVersion????
    const ChunkVersion startingCollectionVersion =
        (existingRoutingInfo ? existingRoutingInfo->getVersion() : ChunkVersion::UNSHARDED());

	//????????????????{}
    const auto refreshFailed =
        [ this, t, dbEntry, nss, refreshAttempt ](WithLock lk, const Status& status) noexcept 
    {
        log() << "Refresh for collection " << nss << " took " << t.millis() << " ms and failed"
              << causedBy(redact(status));

        auto& collections = dbEntry->collections;
        auto it = collections.find(nss.ns());
        invariant(it != collections.end());
        auto& collEntry = it->second;

        // It is possible that the metadata is being changed concurrently, so retry the
        // refresh again
        //cfg????????????????????????????????kMaxInconsistentRoutingInfoRefreshAttempts??
        if (status == ErrorCodes::ConflictingOperationInProgress &&
            refreshAttempt < kMaxInconsistentRoutingInfoRefreshAttempts) {
            //CatalogCache::_scheduleCollectionRefresh ????????
            _scheduleCollectionRefresh(lk, dbEntry, nullptr, nss, refreshAttempt + 1);
        } else {
            // Leave needsRefresh to true so that any subsequent get attempts will kick off
            // another round of refresh
            collEntry.refreshCompletionNotification->set(status);
            collEntry.refreshCompletionNotification = nullptr;
        }
    };

	//??????_cacheLoader.getChunksSince??????????????????
	//????????????getChunksSince????????
    const auto refreshCallback = [ this, t, dbEntry, nss, existingRoutingInfo, refreshFailed ](
        OperationContext * opCtx,
        //swCollAndChunks????????ConfigServerCatalogCacheLoader::getChunksSince
        StatusWith<CatalogCacheLoader::CollectionAndChangedChunks> swCollAndChunks) noexcept {
        std::shared_ptr<ChunkManager> newRoutingInfo;
        try {
			//??cfg??????????????????
            newRoutingInfo = refreshCollectionRoutingInfo(
                opCtx, nss, std::move(existingRoutingInfo), std::move(swCollAndChunks));
        } catch (const DBException& ex) {
            stdx::lock_guard<stdx::mutex> lg(_mutex);
            refreshFailed(lg, ex.toStatus());
            return;
        }

		//??????????????
        stdx::lock_guard<stdx::mutex> lg(_mutex);
        auto& collections = dbEntry->collections;
        auto it = collections.find(nss.ns());
        invariant(it != collections.end());
        auto& collEntry = it->second;

		//??????????????????????needsRefresh????false????????????????????????????????????????????????????????????????????
        collEntry.needsRefresh = false;
        collEntry.refreshCompletionNotification->set(Status::OK());
        collEntry.refreshCompletionNotification = nullptr;

		//??????????????????????????
        if (!newRoutingInfo) {
            log() << "Refresh for collection " << nss << " took " << t.millis()
                  << " and found the collection is not sharded";

            collections.erase(it);
        } else {
        /*
        ??????????????2021-05-31T12:00:39.106+0800 I SH_REFR  [ConfigServerCatalogCacheLoader-27703] Refresh for collection sporthealth.stepsDetail from version 33477|350351||5f9aa6ec3af7fbacfbc99a27 to version 33477|350426||5f9aa6ec3af7fbacfbc99a27 took 364 ms
		????????????????????????
		            const int logLevel = (!existingRoutingInfo || (existingRoutingInfo &&
                                                           routingInfoAfterRefresh->getVersion() !=
                                                               existingRoutingInfo->getVersion()))
                ? 0
                : 1;
            LOG_CATALOG_REFRESH(logLevel)
                << "Refresh for collection " << nss.toString()
                << (existingRoutingInfo
                        ? (" from version " + existingRoutingInfo->getVersion().toString())
                        : "")
                << " to version " << routingInfoAfterRefresh->getVersion().toString() << " took "
                << t.millis() << " ms";
		*/
            log() << "Refresh for collection " << nss << " took " << t.millis()
                  << " ms and found version " << newRoutingInfo->getVersion();

			//??????????????????????routingInfo????????????newRoutingInfo
            collEntry.routingInfo = std::move(newRoutingInfo);
        }
    };

    log() << "Refreshing chunks for collection " << nss << " based on version "
          << startingCollectionVersion;

    try {
		//ConfigServerCatalogCacheLoader::getChunksSince  
		//????????????chunk??????????????refreshCallback????????????????
        _cacheLoader.getChunksSince(nss, startingCollectionVersion, refreshCallback);
    } catch (const DBException& ex) {
        const auto status = ex.toStatus();

        // ConflictingOperationInProgress errors trigger retry of the catalog cache reload logic. If
        // we failed to schedule the asynchronous reload, there is no point in doing another
        // attempt.
        invariant(status != ErrorCodes::ConflictingOperationInProgress);

        stdx::lock_guard<stdx::mutex> lg(_mutex);
        refreshFailed(lg, status);
    }
}

//CachedDatabaseInfo????????????
CachedDatabaseInfo::CachedDatabaseInfo(std::shared_ptr<CatalogCache::DatabaseInfoEntry> db)
    : _db(std::move(db)) {}

const ShardId& CachedDatabaseInfo::primaryId() const {
    return _db->primaryShardId;
}

bool CachedDatabaseInfo::shardingEnabled() const {
    return _db->shardingEnabled;
}

//CachedCollectionRoutingInfo??????
CachedCollectionRoutingInfo::CachedCollectionRoutingInfo(ShardId primaryId,
                                                         std::shared_ptr<ChunkManager> cm)
    : _primaryId(std::move(primaryId)), _cm(std::move(cm)) {}

//CatalogCache::getCollectionRoutingInfo?????????? //CachedCollectionRoutingInfo??????
CachedCollectionRoutingInfo::CachedCollectionRoutingInfo(ShardId primaryId,
                                                         NamespaceString nss,
                                                         std::shared_ptr<Shard> primary)
    : _primaryId(std::move(primaryId)), _nss(std::move(nss)), _primary(std::move(primary)) {}

}  // namespace mongo
