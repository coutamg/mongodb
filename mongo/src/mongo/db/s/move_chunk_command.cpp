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

#include "mongo/client/remote_command_targeter.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/commands.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/repl/repl_client_info.h"
#include "mongo/db/s/chunk_move_write_concern_options.h"
#include "mongo/db/s/collection_metadata.h"
#include "mongo/db/s/collection_sharding_state.h"
#include "mongo/db/s/migration_source_manager.h"
#include "mongo/db/s/move_timing_helper.h"
#include "mongo/db/s/sharding_state.h"
#include "mongo/s/catalog_cache_loader.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/s/migration_secondary_throttle_options.h"
#include "mongo/s/move_chunk_request.h"
#include "mongo/util/concurrency/notification.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"

namespace mongo {

using std::string;

namespace {

/**
 * If the specified status is not OK logs a warning and throws a DBException corresponding to the
 * specified status.
 */
void uassertStatusOKWithWarning(const Status& status) {
    if (!status.isOK()) {
        warning() << "Chunk move failed" << causedBy(redact(status));
        uassertStatusOK(status);
    }
}

// Tests can pause and resume moveChunk's progress at each step by enabling/disabling each failpoint
MONGO_FP_DECLARE(moveChunkHangAtStep1);
MONGO_FP_DECLARE(moveChunkHangAtStep2);
MONGO_FP_DECLARE(moveChunkHangAtStep3);
MONGO_FP_DECLARE(moveChunkHangAtStep4);
MONGO_FP_DECLARE(moveChunkHangAtStep5);
MONGO_FP_DECLARE(moveChunkHangAtStep6);
MONGO_FP_DECLARE(moveChunkHangAtStep7);

//??????????config server??????????moveChunk????  
//????MoveChunkCmd??MoveChunkCommand????????MoveChunkCmd??????????mongo shell????????????????????
//????????configsvr_client::moveChunk??????_configsvrMoveChunk??config server,??config server????
//????movechunk??shard????chunk??????????????MoveChunkCommand::run??????shard????????shard??????

//MoveChunkCommand??shard????movechunk????????????????????????
//MoveChunkCmd??mongos??????????movechunk??????????????????????config server
//ConfigSvrMoveChunkCommand??config server????mongos????????_configsvrMoveChunk??????????????

//????balancer????shard??????????????????????Balancer::_moveChunks->MigrationManager::executeMigrationsForAutoBalance
//????balance??config????????ConfigSvrMoveChunkCommand??????????????Balancer::moveSingleChunk
class MoveChunkCommand : public BasicCommand {
public:
    MoveChunkCommand() : BasicCommand("moveChunk") {}

    void help(std::stringstream& help) const override {
        help << "should not be calling this directly";
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
                               const string& dbname,
                               const BSONObj& cmdObj) override {
        if (!AuthorizationSession::get(client)->isAuthorizedForActionsOnResource(
                ResourcePattern::forClusterResource(), ActionType::internal)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        return Status::OK();
    }

    string parseNs(const string& dbname, const BSONObj& cmdObj) const override {
        return parseNsFullyQualified(dbname, cmdObj);
    }

	//MoveChunkCommand::run
    bool run(OperationContext* opCtx,
             const string& dbname,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) override {
        auto shardingState = ShardingState::get(opCtx);
        uassertStatusOK(shardingState->canAcceptShardedCommands());

		//????moveChunk????????
        const MoveChunkRequest moveChunkRequest = uassertStatusOK(
            MoveChunkRequest::createFromCommand(NamespaceString(parseNs(dbname, cmdObj)), cmdObj));

        // Make sure we're as up-to-date as possible with shard information. This catches the case
        // where we might have changed a shard's host by removing/adding a shard with the same name.
        Grid::get(opCtx)->shardRegistry()->reload(opCtx);

        auto scopedRegisterMigration =
			//ShardingState::registerDonateChunk  
            uassertStatusOK(shardingState->registerDonateChunk(moveChunkRequest));

        Status status = {ErrorCodes::InternalError, "Uninitialized value"};

        // Check if there is an existing migration running and if so, join it
        if (scopedRegisterMigration.mustExecute()) {
            try {
				//????????????????????
                _runImpl(opCtx, moveChunkRequest);
                status = Status::OK();
            } catch (const DBException& e) {
                status = e.toStatus();
            } catch (const std::exception& e) {
                scopedRegisterMigration.complete(
                    {ErrorCodes::InternalError,
                     str::stream() << "Severe error occurred while running moveChunk command: "
                                   << e.what()});
                throw;
            }

            scopedRegisterMigration.complete(status);
        } else {
            status = scopedRegisterMigration.waitForCompletion(opCtx);
        }

		//jumbo chunk????
        if (status == ErrorCodes::ChunkTooBig) {
            // This code is for compatibility with pre-3.2 balancer, which does not recognize the
            // ChunkTooBig error code and instead uses the "chunkTooBig" field in the response,
            // and the 3.4 shard, which failed to set the ChunkTooBig status code.
            // TODO: Remove after 3.6 is released.
            result.appendBool("chunkTooBig", true);
            return appendCommandStatus(result, status);
        }

        uassertStatusOK(status);

        if (moveChunkRequest.getWaitForDelete()) {
            // Ensure we capture the latest opTime in the system, since range deletion happens
            // asynchronously with a different OperationContext. This must be done after the above
            // join, because each caller must set the opTime to wait for writeConcern for on its own
            // OperationContext.
            // TODO (SERVER-30183): If this moveChunk joined an active moveChunk that did not have
            // waitForDelete=true, the captured opTime may not reflect all the deletes.
            repl::ReplClientInfo::forClient(opCtx->getClient()).setLastOpToSystemLastOpTime(opCtx);
        }

        return true;
    }

private:
	//MoveChunkCommand::run??????   //MoveChunkCommand::_runImpl
    static void _runImpl(OperationContext* opCtx, const MoveChunkRequest& moveChunkRequest) {
        const auto writeConcernForRangeDeleter =
			//????moveChunk??????????????
            uassertStatusOK(ChunkMoveWriteConcernOptions::getEffectiveWriteConcern(
                opCtx, moveChunkRequest.getSecondaryThrottle()));

        // Resolve the donor and recipient shards and their connection string
        auto const shardRegistry = Grid::get(opCtx)->shardRegistry();

        const auto donorConnStr =
			//??????????????????????????????
            uassertStatusOK(shardRegistry->getShard(opCtx, moveChunkRequest.getFromShardId()))
                ->getConnString();
        const auto recipientHost = uassertStatusOK([&] {
			//????shard????
            auto recipientShard =
                uassertStatusOK(shardRegistry->getShard(opCtx, moveChunkRequest.getToShardId()));

			//??????????????????
            return recipientShard->getTargeter()->findHostNoWait(
                ReadPreferenceSetting{ReadPreference::PrimaryOnly});
        }());

        string unusedErrMsg;
		//????MoveTimingHelper  ????????????????????????
        MoveTimingHelper moveTimingHelper(opCtx,
                                          "from",
                                          moveChunkRequest.getNss().ns(),
                                          moveChunkRequest.getMinKey(),
                                          moveChunkRequest.getMaxKey(),
                                          6,  // Total number of steps
                                          &unusedErrMsg,
                                          moveChunkRequest.getToShardId(),
                                          moveChunkRequest.getFromShardId());

		//??????kCreated????
		//MoveTimingHelper::done
        moveTimingHelper.done(1);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep1);
		//????MigrationSourceManager
        MigrationSourceManager migrationSourceManager(
            opCtx, moveChunkRequest, donorConnStr, recipientHost);

		//??????kCloning????
        moveTimingHelper.done(2);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep2);
		//????_recvChunkStart??????kCloning??????????????????????????????chunk????
        uassertStatusOKWithWarning(migrationSourceManager.startClone(opCtx));

		//??????kCloneCaughtUp????
        moveTimingHelper.done(3);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep3);
		//????????????????????????????????????????????????????????????
        uassertStatusOKWithWarning(migrationSourceManager.awaitToCatchUp(opCtx));

		//??????kCriticalSection????
		moveTimingHelper.done(4);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep4);
		//??????????????????????????????
        uassertStatusOKWithWarning(migrationSourceManager.enterCriticalSection(opCtx));
		//????_recvChunkCommit??????????????????????????????????????????????????????????????
		uassertStatusOKWithWarning(migrationSourceManager.commitChunkOnRecipient(opCtx));

		//??????kCloneCompleted????
		moveTimingHelper.done(5);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep5);
		////????"_configsvrCommitChunkMigration"????????????????????config??????
		//chunk??????????????config??????????????????????????????????????????chunk????
        uassertStatusOKWithWarning(migrationSourceManager.commitChunkMetadataOnConfig(opCtx));

		moveTimingHelper.done(6);
        MONGO_FAIL_POINT_PAUSE_WHILE_SET(moveChunkHangAtStep6);
    }

} moveChunkCmd;

}  // namespace
}  // namespace mongo
