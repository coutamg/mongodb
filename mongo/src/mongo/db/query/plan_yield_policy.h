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

#include "mongo/db/catalog/collection.h"
#include "mongo/db/query/plan_executor.h"
#include "mongo/stdx/functional.h"
#include "mongo/util/elapsed_tracker.h"

namespace mongo {

class ClockSource;
class RecordFetcher;

//参考http://www.mongoing.com/archives/5476  让出请求查询的CPU时间片相关
//构造赋值参考makeYieldPolicy，AlwaysTimeOutYieldPolicy、AlwaysPlanKilledYieldPolicy继承该类
class PlanYieldPolicy {
public:
    virtual ~PlanYieldPolicy() {}

    PlanYieldPolicy(PlanExecutor* exec, PlanExecutor::YieldPolicy policy);

    /**
     * Only used in dbtests since we don't have access to a PlanExecutor. Since we don't have
     * access to the PlanExecutor to grab a ClockSource from, we pass in a ClockSource directly
     * in the constructor instead.
     */
    PlanYieldPolicy(PlanExecutor::YieldPolicy policy, ClockSource* cs);

    /**
     * Used by YIELD_AUTO plan executors in order to check whether it is time to yield.
     * PlanExecutors give up their locks periodically in order to be fair to other
     * threads.
     */
    virtual bool shouldYield();

    /**
     * Resets the yield timer so that we wait for a while before yielding again.
     */
    void resetTimer();

    /**
     * Used to cause a plan executor to release locks or storage engine state. The PlanExecutor must
     * *not* be in saved state. Handles calls to save/restore state internally.
     *
     * If 'fetcher' is non-NULL, then we are yielding because the storage engine told us
     * that we will page fault on this record. We use 'fetcher' to retrieve the record
     * after we give up our locks.
     *
     * Returns Status::OK() if the executor was restored successfully and is still alive. Returns
     * ErrorCodes::QueryPlanKilled if the executor got killed during yield, and
     * ErrorCodes::ExceededTimeLimit if the operation has exceeded the time limit.
     */
    virtual Status yield(RecordFetcher* fetcher = NULL);

    /**
     * More generic version of yield() above.  This version calls 'beforeYieldingFn' immediately
     * before locks are yielded (if they are), and 'whileYieldingFn' before locks are restored.
     */
    virtual Status yield(stdx::function<void()> beforeYieldingFn,
                         stdx::function<void()> whileYieldingFn);

    /**
     * All calls to shouldYield() will return true until the next call to yield.
     */
    void forceYield() {
        dassert(canAutoYield());
        _forceYield = true;
    }

    /**
     * Returns true if there is a possibility that a collection lock will be yielded at some point
     * during this PlanExecutor's lifetime.
     */
    //生效见PlanExecutor::PlanExecutor
    bool canReleaseLocksDuringExecution() const {
        switch (_policy) {
            case PlanExecutor::YIELD_AUTO:
            case PlanExecutor::YIELD_MANUAL:
            case PlanExecutor::ALWAYS_TIME_OUT:
            case PlanExecutor::ALWAYS_MARK_KILLED: {
                return true;
            }
            case PlanExecutor::NO_YIELD:
            case PlanExecutor::WRITE_CONFLICT_RETRY_ONLY: {
                return false;
            }
        }
        MONGO_UNREACHABLE;
    }

    /**
     * Returns true if this yield policy performs automatic yielding. Note 'yielding' here refers to
     * either releasing storage engine resources via abandonSnapshot() OR yielding LockManager
     * locks.
     */
    //PlanYieldPolicy::shouldYield()  MultiPlanStage::workAllPlans  CachedPlanStage::pickBestPlan调用
    bool canAutoYield() const {
        switch (_policy) {
            case PlanExecutor::YIELD_AUTO:
            case PlanExecutor::WRITE_CONFLICT_RETRY_ONLY:
            case PlanExecutor::ALWAYS_TIME_OUT:
            case PlanExecutor::ALWAYS_MARK_KILLED: {
                return true;
            }
            case PlanExecutor::NO_YIELD:
            case PlanExecutor::YIELD_MANUAL:
                return false;
        }
        MONGO_UNREACHABLE;
    }

    PlanExecutor::YieldPolicy getPolicy() const {
        return _policy;
    }

private:
    
    const PlanExecutor::YieldPolicy _policy;

    bool _forceYield;
    //定时器相关，定时时间到需要让出CPU
    ElapsedTracker _elapsedTracker;

    // The plan executor which this yield policy is responsible for yielding. Must
    // not outlive the plan executor.
    PlanExecutor* const _planYielding;
};

}  // namespace mongo
