/**
 *    Copyright (C) 2017 10gen Inc.
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

#include "mongo/db/catalog/collection_info_cache.h"

namespace mongo {
namespace {
stdx::function<CollectionInfoCache::factory_function_type> factory;
}  // namespace

CollectionInfoCache::Impl::~Impl() = default;

//InitializeCollectionInfoCacheFactory中完成初始化注册
void CollectionInfoCache::registerFactory(decltype(factory) newFactory) {
    factory = std::move(newFactory);
}

auto CollectionInfoCache::makeImpl(Collection* const collection, const NamespaceString& ns)
    -> std::unique_ptr<Impl> {
    //也就是调用InitializeCollectionInfoCacheFactory
    //注册Impl为CollectionInfoCacheImpl
    return factory(collection, ns);
}

void CollectionInfoCache::TUHook::hook() noexcept {}
}  // namespace mongo
