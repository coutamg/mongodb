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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/bson/util/bson_extract.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/commands.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/s/balancer/balancer.h"
#include "mongo/s/request_types/balance_chunk_request_type.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {
namespace {

using std::string;
using str::stream;

/*
mongos执行:db.runCommand({movePrimary:"test", to:"XX_gQmJGvRW_shard_2"})

{
原分片打印
2020-09-10T20:41:10.672+0800 I COMMAND  [conn378169] dropDatabase test - starting
2020-09-10T20:41:10.672+0800 I COMMAND  [conn378169] dropDatabase test - dropping 2 collections
2020-09-10T20:41:10.672+0800 I COMMAND  [conn378169] dropDatabase test - dropping collection: test.item_commit_info
2020-09-10T20:41:10.672+0800 I STORAGE  [conn378169] dropCollection: test.item_commit_info (cf56fa2d-6d8b-4320-8a4b-0119aa13125a) - renaming to drop-pending collection: test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 }
2020-09-10T20:41:10.674+0800 I COMMAND  [conn378169] dropDatabase test - dropping collection: test.test1
2020-09-10T20:41:10.674+0800 I STORAGE  [conn378169] dropCollection: test.test1 (44fb57d7-b804-424f-9695-c4d8aac078f9) - renaming to drop-pending collection: test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 }
2020-09-10T20:41:10.681+0800 I REPL     [replication-8766] Completing collection drop for test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 } (notification optime: { ts: Timestamp(1599741670, 2990), t: 13 })
2020-09-10T20:41:10.681+0800 I STORAGE  [replication-8766] Finishing collection drop for test.system.drop.1599741670i2990t13.item_commit_info (cf56fa2d-6d8b-4320-8a4b-0119aa13125a).
2020-09-10T20:41:10.682+0800 I REPL     [replication-8768] Completing collection drop for test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3000), t: 13 })
2020-09-10T20:41:10.682+0800 I COMMAND  [conn378169] dropDatabase test - successfully dropped 2 collections (most recent drop optime: { ts: Timestamp(1599741670, 3000), t: 13 }) after 7ms. dropping database
2020-09-10T20:41:10.710+0800 I REPL     [replication-8768] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3000), t: 13 })
2020-09-10T20:41:10.728+0800 I REPL     [replication-8766] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3011), t: 13 })
2020-09-10T20:41:10.728+0800 I REPL     [replication-8769] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3012), t: 13 })
2020-09-10T20:41:10.730+0800 I REPL     [replication-8770] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3048), t: 13 })
2020-09-10T20:41:10.731+0800 I COMMAND  [conn378169] dropDatabase test - finished

目的分片主节点打印：
2020-09-10T20:41:10.559+0800 I STORAGE  [conn2055546] createCollection: test.item_commit_info with generated UUID: 5d22309b-ef17-4d01-adf2-b9f494ee143a
2020-09-10T20:41:10.579+0800 I STORAGE  [conn2055546] createCollection: test.test1 with generated UUID: 649b14a7-7a75-4ef1-a528-91f89adc046e
2020-09-10T20:41:10.636+0800 I STORAGE  [conn2055546] copying indexes for: { name: "item_commit_info", type: "collection", options: {}, info: { readOnly: false, uuid: UUID("cf56fa2d-6d8b-4320-8a4b-0119aa13125a") }, idIndex: { v: 2, key: { _id: 1 }, name: "_id_", ns: "test.item_commit_info" } }
2020-09-10T20:41:10.645+0800 I INDEX    [conn2055546] build index on: test.item_commit_info properties: { v: 2, unique: true, key: { tag: 1.0 }, name: "tag_1", ns: "test.item_commit_info" }
2020-09-10T20:41:10.645+0800 I INDEX    [conn2055546]    building index using bulk method; build may temporarily use up to 250 megabytes of RAM
2020-09-10T20:41:10.652+0800 I INDEX    [conn2055546] build index on: test.item_commit_info properties: { v: 2, unique: true, key: { tag2: 1.0 }, name: "testindex", ns: "test.item_commit_info" }
2020-09-10T20:41:10.652+0800 I INDEX    [conn2055546]    building index using bulk method; build may temporarily use up to 250 megabytes of RAM
2020-09-10T20:41:10.655+0800 I INDEX    [conn2055546] build index done.  scanned 1 total records. 0 secs
2020-09-10T20:41:10.655+0800 I STORAGE  [conn2055546] copying indexes for: { name: "test1", type: "collection", options: {}, info: { readOnly: false, uuid: UUID("44fb57d7-b804-424f-9695-c4d8aac078f9") }, idIndex: { v: 2, key: { _id: 1 }, name: "_id_", ns: "test.test1" } }
2020-09-10T20:41:10.668+0800 I COMMAND  [conn2055546] command test.$cmd appName: "MongoDB Shell" command: clone { clone: "opush_gQmJGvRW_shard_1/10.36.116.42:20001,10.37.72.102:20001,10.37.76.22:20001", collsToIgnore: [], bypassDocumentValidation: true, writeConcern: { w: "majority", wtimeout: 60000 }, $db: "test", $clusterTime: { clusterTime: Timestamp(1599741670, 2347), signature: { hash: BinData(0, C29F3B6CBB7BB2A931E07D1CFA6E71953A464885), keyId: 6829778851464216577 } }, $client: { application: { name: "MongoDB Shell" }, driver: { name: "MongoDB Internal Client", version: "3.6.14" }, os: { type: "Linux", name: "CentOS release 6.8 (Final)", architecture: "x86_64", version: "Kernel 2.6.32-642.el6.x86_64" }, mongos: { host: "bjht7266:20003", client: "10.35.150.17:44094", version: "3.6.10" } }, $configServerState: { opTime: { ts: Timestamp(1599741670, 2347), t: 7 } } } numYields:0 reslen:353 locks:{ Global: { acquireCount: { r: 17, w: 15, W: 2 }, acquireWaitCount: { W: 2 }, timeAcquiringMicros: { W: 6420 } }, Database: { acquireCount: { w: 10, W: 5 } }, oplog: { acquireCount: { w: 10 } } } protocol:op_msg 123ms

*/
//源分片收到config server发送过来的moveChunk命令  
//注意MoveChunkCmd和MoveChunkCommand的区别，MoveChunkCmd为代理收到mongo shell等客户端的处理流程，
//然后调用configsvr_client::moveChunk，发送_configsvrMoveChunk给config server,由config server统一
//发送movechunk给shard执行chunk操作，从而执行MoveChunkCommand::run来完成shard见真正的shard间迁移

//MoveChunkCommand为shard收到movechunk命令的真正数据迁移的入口
//MoveChunkCmd为mongos收到客户端movechunk命令的处理流程，转发给config server
//ConfigSvrMoveChunkCommand为config server收到mongos发送来的_configsvrMoveChunk命令的处理流程

//自动balancer触发shard做真正的数据迁移入口在Balancer::_moveChunks->MigrationManager::executeMigrationsForAutoBalance
//手动balance，config收到代理ConfigSvrMoveChunkCommand命令后迁移入口Balancer::moveSingleChunk


//mongos对应的接口如下:
//BalanceChunkRequest::serializeToMoveCommandForConfig      
//BalanceChunkRequest::serializeToRebalanceCommandForConfig
class ConfigSvrMoveChunkCommand : public BasicCommand {
public:
    ConfigSvrMoveChunkCommand() : BasicCommand("_configsvrMoveChunk") {}

    void help(std::stringstream& help) const override {
        help << "Internal command, which is exported by the sharding config server. Do not call "
                "directly. Requests the balancer to move or rebalance a single chunk.";
    }

    bool slaveOk() const override {
        return false;
    }

    bool adminOnly() const override {
        return true;
    }

    bool supportsWriteConcern(const BSONObj& cmd) const override {
        return true;
    }

    Status checkAuthForCommand(Client* client,
                               const std::string& dbname,
                               const BSONObj& cmdObj) override {
        if (!AuthorizationSession::get(client)->isAuthorizedForActionsOnResource(
                ResourcePattern::forClusterResource(), ActionType::internal)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        return Status::OK();
    }

    bool run(OperationContext* opCtx,
             const std::string& unusedDbName,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) override {
        auto request = uassertStatusOK(BalanceChunkRequest::parseFromConfigCommand(cmdObj));

		//手动movechunk
        if (request.hasToShardId()) {
			//Balancer::moveSingleChunk
            uassertStatusOK(Balancer::get(opCtx)->moveSingleChunk(opCtx,
                                                                  request.getChunk(),
                                                                  request.getToShardId(),
                                                                  request.getMaxChunkSizeBytes(),
                                                                  request.getSecondaryThrottle(),
                                                                  request.getWaitForDelete()));
        } else { //自动movechunk
        	//Balancer::rebalanceSingleChunk
            uassertStatusOK(Balancer::get(opCtx)->rebalanceSingleChunk(opCtx, request.getChunk()));
        }

        return true;
    }

} configSvrMoveChunk;

}  // namespace
}  // namespace mongo
