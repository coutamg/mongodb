/**
 *    Copyright (C) 2013 10gen Inc.
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

#include "mongo/bson/timestamp.h"
#include "mongo/db/record_id.h"

namespace mongo {

class Collection;

//buildStages中使用
struct CollectionScanParams {
    //排序顺序，增还是减
    enum Direction {
        FORWARD = 1,
        BACKWARD = -1,
    };

    // What collection?
    // not owned
    const Collection* collection = nullptr;

    // isNull by default.  If you specify any value for this, you're responsible for the RecordId
    // not being invalidated before the first call to work(...).
    RecordId start;

    // If present, the collection scan will stop and return EOF the first time it sees a document
    // that does not pass the filter and has 'ts' greater than 'maxTs'.
    boost::optional<Timestamp> maxTs;

    Direction direction = FORWARD;

    // Do we want the scan to be 'tailable'?  Only meaningful if the collection is capped.
    bool tailable = false;

    // Should we keep track of the timestamp of the latest oplog entry we've seen? This information
    // is needed to merge cursors from the oplog in order of operation time when reading the oplog
    // across a sharded cluster.
    bool shouldTrackLatestOplogTimestamp = false;

    // Once the first matching document is found, assume that all documents after it must match.
    // This is useful for oplog queries where we know we will see records ordered by the ts field.
    bool stopApplyingFilterAfterFirstMatch = false;

    // If non-zero, how many documents will we look at?
    size_t maxScan = 0; //db.collection.find( { $query: { <query> }, $maxScan: <number> } 
};

}  // namespace mongo
