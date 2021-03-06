/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include "mongo/platform/basic.h"

#include "mongo/db/catalog/database.h"


namespace mongo {
Database::Impl::~Impl() = default;

namespace {
stdx::function<Database::factory_function_type> factory;
}  // namespace

void Database::registerFactory(decltype(factory) newFactory) {
    factory = std::move(newFactory);
}

//Database初始化构造调用
auto Database::makeImpl(Database* const this_,
                        OperationContext* const opCtx,
                        const StringData name,
                        DatabaseCatalogEntry* const dbEntry) -> std::unique_ptr<Impl> {
	//也就是registerFactory注册的回调，该回调生成类DatabaseImpl
	return factory(this_, opCtx, name, dbEntry);
}

void Database::TUHook::hook() noexcept {}

//drop_database.cpp中的dropDatabase和DatabaseImpl::dropDatabase  dropDatabaseImpl什么区别？需要进一步分析
namespace {
stdx::function<decltype(Database::dropDatabase)> dropDatabaseImpl;
}

void Database::dropDatabase(OperationContext* const opCtx, Database* const db) {
	//DatabaseImpl::dropDatabase
	return dropDatabaseImpl(opCtx, db);
}

//InitializeDropDatabaseImpl中调用，把dropDatabaseImpl赋值为DatabaseImpl::dropDatabase
void Database::registerDropDatabaseImpl(stdx::function<decltype(dropDatabase)> impl) {
    dropDatabaseImpl = std::move(impl);
}

namespace {
stdx::function<decltype(userCreateNS)> userCreateNSImpl;
stdx::function<decltype(dropAllDatabasesExceptLocal)> dropAllDatabasesExceptLocalImpl;
}  // namespace
}  // namespace mongo

/*Status userCreateNS(OperationContext* opCtx,
                    Database* db,
                    StringData ns,
                    BSONObj options,
                    CollectionOptions::ParseKind parseKind = CollectionOptions::parseForCommand,
                    bool createDefaultIndexes = true,
                    const BSONObj& idIndex = BSONObj());
*/
//手动建表流程：CmdCreate::run->createCollection
//直接写入数据的时候会建表流程：insertBatchAndHandleErrors->makeCollection->mongo::userCreateNS

// 没有则创建集合及相关的索引文件  
auto mongo::userCreateNS(OperationContext* const opCtx,
                         Database* const db,
                         const StringData ns,
                         const BSONObj options,
                         const CollectionOptions::ParseKind parseKind,
                         const bool createDefaultIndexes,
                         const BSONObj& idIndex) -> Status {
    return userCreateNSImpl(opCtx, db, ns, options, parseKind, createDefaultIndexes, idIndex);
}

void mongo::registerUserCreateNSImpl(stdx::function<decltype(userCreateNS)> impl) {
    userCreateNSImpl = std::move(impl);
}

void mongo::dropAllDatabasesExceptLocal(OperationContext* const opCtx) {
    return dropAllDatabasesExceptLocalImpl(opCtx);
}

/**
 * Registers an implementation of `dropAllDatabaseExceptLocal` for use by library clients.
 * This is necessary to allow `catalog/database` to be a vtable edge.
 * @param impl Implementation of `dropAllDatabaseExceptLocal` to install.
 * @note This call is not thread safe.
 */
void mongo::registerDropAllDatabasesExceptLocalImpl(
    stdx::function<decltype(dropAllDatabasesExceptLocal)> impl) {
    dropAllDatabasesExceptLocalImpl = std::move(impl);
}
