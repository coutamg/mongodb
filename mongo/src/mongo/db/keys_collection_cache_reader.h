/**
 *    Copyright (C) 2017 MongoDB, Inc.
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

#include <map>

#include "mongo/db/keys_collection_cache.h"
#include "mongo/stdx/mutex.h"

namespace mongo {

class KeysCollectionClient;

/**
 * Keeps a local cache of the keys with the ability to refresh.
 *
 * Note: This assumes that user does not manually update the keys collection.
 */

//KeysCollectionManagerSharding::enableKeyGenerator中构造KeysCollectionCacheReaderAndUpdater或者KeysCollectionCacheReader
//KeysCollectionCacheReaderAndUpdater继承KeysCollectionCacheReader
//KeysCollectionManagerSharding._keysCache为该类型

class KeysCollectionCacheReader : public KeysCollectionCache {
public:
    KeysCollectionCacheReader(std::string purpose, KeysCollectionClient* client);
    ~KeysCollectionCacheReader() = default;

    /**
     * Check if there are new documents expiresAt > latestKeyDoc.expiresAt.
     */
    StatusWith<KeysCollectionDocument> refresh(OperationContext* opCtx) override;

    StatusWith<KeysCollectionDocument> getKey(const LogicalTime& forThisTime) override;
    StatusWith<KeysCollectionDocument> getKeyById(long long keyId,
                                                  const LogicalTime& forThisTime) override;

    /**
     * Resets the cache of keys if the client doesnt allow readConcern level:majority reads.
     * This method intended to be called on the rollback of the node.
     */
    void resetCache();

private:
    const std::string _purpose;
    //KeysCollectionClientDirect  KeysCollectionClientSharded 
    KeysCollectionClient* const _client;

    stdx::mutex _cacheMutex;
    //key doc缓存到内存中，见KeysCollectionCacheReader::refresh
    std::map<LogicalTime, KeysCollectionDocument> _cache;  // expiresAt -> KeysDocument
};

}  // namespace mongo
