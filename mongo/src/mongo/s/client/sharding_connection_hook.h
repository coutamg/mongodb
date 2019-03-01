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

#include "mongo/client/connpool.h"
#include "mongo/rpc/metadata.h"
#include "mongo/s/sharding_egress_metadata_hook.h"

namespace mongo {

class DBClientBase;

/**
 * Intercepts creation of sharded connections and transparently performs the internal
 * authentication on them.
 */ //runMongosServer中构造使用
class ShardingConnectionHook : public DBConnectionHook {
public:
    ShardingConnectionHook(bool shardedConnections,
                           std::unique_ptr<rpc::EgressMetadataHook> egressHook);

    void onCreate(DBClientBase* conn) override;
    void onDestroy(DBClientBase* conn) override;
    void onRelease(DBClientBase* conn) override;

private:
    bool _shardedConnections; //是否sharde链接

    // Use the implementation of the metadata readers and writers in ShardingEgressMetadataHook,
    // since that is the hook for Network Interface ASIO and this hook is to be deprecated.
    std::unique_ptr<rpc::EgressMetadataHook> _egressHook;
};

}  // namespace mongo
