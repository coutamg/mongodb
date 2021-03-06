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

#include "mongo/platform/basic.h"

#include "mongo/base/init.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands/server_status.h"
#include "mongo/s/balancer_configuration.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"

namespace mongo {
namespace {

/*
mongos> db.serverStatus().sharding
{
        "configsvrConnectionString" : "xxxxconfigdb/10.xx.xx.238:20014,10.xx.xx.234:20009,10.xx.xx.91:20016",
        "lastSeenConfigServerOpTime" : {
                "ts" : Timestamp(1623044905, 3622),
                "t" : NumberLong(12)
        },
        "maxChunkSizeInBytes" : NumberLong(67108864)
}
mongos> 
*/
//db.serverstatus.sharding
class ShardingServerStatus final : public ServerStatusSection {
public:
    ShardingServerStatus() : ServerStatusSection("sharding") {}

    bool includeByDefault() const override {
        return true;
    }

    BSONObj generateSection(OperationContext* opCtx,
                            const BSONElement& configElement) const override {
        auto shardRegistry = Grid::get(opCtx)->shardRegistry();
        invariant(shardRegistry);

        BSONObjBuilder result;
        result.append("configsvrConnectionString",
                      shardRegistry->getConfigServerConnectionString().toString());

        Grid::get(opCtx)->configOpTime().append(&result, "lastSeenConfigServerOpTime");

        long long maxChunkSizeInBytes =
            Grid::get(opCtx)->getBalancerConfiguration()->getMaxChunkSizeBytes();
        result.append("maxChunkSizeInBytes", maxChunkSizeInBytes);

        return result.obj();
    }
};

MONGO_INITIALIZER(ShardingServerStatusSection)(InitializerContext* context) {
    new ShardingServerStatus();

    return Status::OK();
}

}  // namespace
}  // namespace mongo
