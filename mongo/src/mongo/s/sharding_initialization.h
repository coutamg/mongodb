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

#pragma once

#include <boost/optional.hpp>

#include "mongo/base/string_data.h"
#include "mongo/bson/oid.h"
#include "mongo/stdx/functional.h"
#include "mongo/stdx/memory.h"

namespace mongo {

class CatalogCache;
class ConnectionString;
class OperationContext;
class ShardFactory;
class Status;
class ShardingCatalogClient;

namespace executor {
class NetworkInterface;
class TaskExecutor;
}  // namespace executor

namespace rpc {
class EgressMetadataHook;
using ShardingEgressMetadataHookBuilder = stdx::function<std::unique_ptr<EgressMetadataHook>()>;
}  // namespace rpc

/**
 * Fixed process identifier for the dist lock manager running on a config server.
 */ //cfgserver??????lockid
constexpr auto kDistLockProcessIdForConfigServer = "ConfigServer"_sd;

/**
 * Generates a uniform string to be used as a process id for the distributed lock manager.
 */ //????opctx????????lockid
std::string generateDistLockProcessId(OperationContext* opCtx);

/**
 * Constructs a TaskExecutor which contains the required configuration for the sharding subsystem.
 */
std::unique_ptr<executor::TaskExecutor> makeShardingTaskExecutor(
    std::unique_ptr<executor::NetworkInterface> net);

/**
 * Takes in the connection string for reaching the config servers and initializes the global
 * ShardingCatalogClient, ShardingCatalogManager, ShardRegistry, and Grid objects.
 */
Status initializeGlobalShardingState(OperationContext* opCtx,
                                     const ConnectionString& configCS,
                                     StringData distLockProcessId,
                                     std::unique_ptr<ShardFactory> shardFactory,
                                     std::unique_ptr<CatalogCache> catalogCache,
                                     rpc::ShardingEgressMetadataHookBuilder hookBuilder,
                                     boost::optional<size_t> taskExecutorPoolSize);


/**
 * Loads cluster ID and waits for the reload of the Shard Registry.
*/

Status waitForShardRegistryReload(OperationContext* opCtx);

}  // namespace mongo
