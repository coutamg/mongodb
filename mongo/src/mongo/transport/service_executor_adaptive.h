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

#pragma once

#include <vector>

#include "mongo/db/service_context.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/list.h"
#include "mongo/stdx/thread.h"
#include "mongo/transport/service_executor.h"
#include "mongo/util/tick_source.h"

#include <asio.hpp>

namespace mongo {
namespace transport {

/**
 * This is an ASIO-based adaptive ServiceExecutor. It guarantees that threads will not become stuck
 * or deadlocked longer that its configured timeout and that idle threads will terminate themselves
 * if they spend more than its configure idle threshold idle.
 * ????????ASIO????????????????????????????????????????????????????????????????????????????????????????????
 * createWithConfig??????????
 */
/*
"adaptive") : <ServiceExecutorAdaptive>( ??????boost.asio????????????????????????????????????????????????????????????
??????????????????workload??????????????????????????????????????????????????????????????????????????????????????????
??????????????????

"synchronous"): <ServiceExecutorSynchronous>(ctx));  ????????????????????????????????????????????????recv/send??????
}
*/

//??????????TransportLayerManager::createWithConfig,??????????ServiceContext._serviceExecutor
//ServiceExecutorSynchronous????????????????????ServiceExecutorAdaptive????????????????????????????????????????????????????????????
class ServiceExecutorAdaptive : public ServiceExecutor {
public:
    struct Options {
        virtual ~Options() = default;
        // The minimum number of threads the executor will keep running to service tasks.
        virtual int reservedThreads() const = 0;

        // The amount of time each worker thread runs before considering exiting because of
        // idleness.
        virtual Milliseconds workerThreadRunTime() const = 0;

        // workerThreadRuntime() is offset by a random value between -jitter and +jitter to prevent
        // thundering herds
        virtual int runTimeJitter() const = 0;

        // The amount of time the controller thread will wait before checking for stuck threads
        // to guarantee forward progress
        virtual Milliseconds stuckThreadTimeout() const = 0;

        // The maximum allowed latency between when a task is scheduled and a thread is started to
        // service it.
        virtual Microseconds maxQueueLatency() const = 0;

        // Threads that spend less than this threshold doing work during their workerThreadRunTime
        // period will exit
        virtual int idlePctThreshold() const = 0;

        // The maximum allowable depth of recursion for tasks scheduled with the MayRecurse flag
        // before stack unwinding is forced.
        virtual int recursionLimit() const = 0;
    };

    explicit ServiceExecutorAdaptive(ServiceContext* ctx, std::shared_ptr<asio::io_context> ioCtx);
    explicit ServiceExecutorAdaptive(ServiceContext* ctx,
                                     std::shared_ptr<asio::io_context> ioCtx,
                                     std::unique_ptr<Options> config);

    ServiceExecutorAdaptive(ServiceExecutorAdaptive&&) = default;
    ServiceExecutorAdaptive& operator=(ServiceExecutorAdaptive&&) = default;
    virtual ~ServiceExecutorAdaptive();

    Status start() final;
    Status shutdown(Milliseconds timeout) final;
    Status schedule(Task task, ScheduleFlags flags) final;

    Mode transportMode() const final {
        return Mode::kAsynchronous;
    }

    void appendStats(BSONObjBuilder* bob) const final;

    int threadsRunning() {
        return _threadsRunning.load();
    }

private:
    //????ticks????????????????????
    class TickTimer {
    public:
        explicit TickTimer(TickSource* tickSource)
            : _tickSource(tickSource),
              //1000????????1s??????1000??ticks????????1??ticks????1ms
              _ticksPerMillisecond(_tickSource->getTicksPerSecond() / 1000),
              //??????????????ticks
              _start(_tickSource->getTicks()) {
            invariant(_ticksPerMillisecond > 0);
        }

        //start??????????????
        TickSource::Tick sinceStartTicks() const {
            return _tickSource->getTicks() - _start.load();
        }

        //??ms??????????????????????????ticks??????????????ticks ms
        Milliseconds sinceStart() const {
            return Milliseconds{sinceStartTicks() / _ticksPerMillisecond};
        }

        //????????tick
        void reset() {
            _start.store(_tickSource->getTicks());
        }

    private:
        TickSource* const _tickSource;
        const TickSource::Tick _ticksPerMillisecond;
        AtomicWord<TickSource::Tick> _start;
    };

    class CumulativeTickTimer {
    public:
        CumulativeTickTimer(TickSource* ts) : _timer(ts) {}

        //??????????????_accumulator
        TickSource::Tick markStopped() {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            invariant(_running);
            _running = false;
            auto curTime = _timer.sinceStartTicks();
            _accumulator += curTime;
            return curTime;
        }

        //????????????
        void markRunning() {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            invariant(!_running);
            _timer.reset();
            _running = true;
        }

        //????????????????
        TickSource::Tick totalTime() const {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            if (!_running)
                return _accumulator;
            return _timer.sinceStartTicks() + _accumulator;
        }

    private:
        //????????????????
        TickTimer _timer;
        mutable stdx::mutex _mutex;
        //??????tick
        TickSource::Tick _accumulator = 0;
        bool _running = false;
    };

    //????????????  ????:????????????????????????????????????????+??????????????
    struct ThreadState {
        ThreadState(TickSource* ts) : running(ts), executing(ts) {}

        //??????????????????????????????IO??????????????????????????task??????,????ServiceExecutorAdaptive::_workerThreadRoutine 
        CumulativeTickTimer running;
        //??????????????task??????????????????worker????????????????????????????????????ServiceStateMachine::_scheduleNextWithGuard
        TickSource::Tick executingCurRun;
        //????????task??????????????????ServiceExecutorAdaptive::schedule
        CumulativeTickTimer executing;
        //??????????????????????????
        int recursionDepth = 0;
    };

    using ThreadList = stdx::list<ThreadState>;

    void _startWorkerThread();
    void _workerThreadRoutine(int threadId, ThreadList::iterator it);
    void _controllerThreadRoutine();
    bool _isStarved() const;
    Milliseconds _getThreadJitter() const;

    enum class ThreadTimer { Running, Executing };
    TickSource::Tick _getThreadTimerTotal(ThreadTimer which) const;

/*
//accept????????????????????????
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//????read write????????????????????????
//mongodb??ServiceExecutorAdaptive::schedule????->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb??ServiceExecutorAdaptive::schedule????->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//????????read write????????????????????????
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one
//->scheduler::do_wait_one????
//mongodb??ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for
//->io_context::run_one_until->schedule::wait_one
		|
		|1.????????????????????(??????mongodb??TransportLayerASIO._workerIOContext  TransportLayerASIO._acceptorIOContext??????????)
		|2.??????????1??????????????????????????TransportLayerASIO::_acceptConnection??TransportLayerASIO::ASIOSourceTicket::fillImpl??
		|  TransportLayerASIO::ASIOSinkTicket::fillImpl????????????????????????????epoll????(????????????)
		|
	    \|/
//accept????????????epoll????????????:reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op
//??????epoll????????????:reactive_descriptor_service::async_read_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
//??????epoll????????????:reactive_descriptor_service::async_write_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
*/

    //TransportLayerManager::createWithConfig????????????????????????????ServiceExecutorAdaptive::schedule 
    //??????TransportLayerASIO._workerIOContext  adaptive??????????????????????accept????????????????IO??????
    std::shared_ptr<asio::io_context> _ioContext; //????ASIO????io_service 
    //TransportLayerManager::createWithConfig????????
    //????ServerParameterOptions
    std::unique_ptr<Options> _config;
    //??????????????
    mutable stdx::mutex _threadsMutex;
    ThreadList _threads;
    //worker-controller thread   ServiceExecutorAdaptive::start??????
    stdx::thread _controllerThread;

    //TransportLayerManager::createWithConfig????????
    //????????????
    TickSource* const _tickSource;
    //??????????shutdown????????false??????????
    AtomicWord<bool> _isRunning{false};

    // These counters are used to detect stuck threads and high task queuing.
	//kTotalQueued:??????????????,??????????ServiceStateMachine::_scheduleNextWithGuard->ServiceExecutorAdaptive::schedule??????
    //kExecutorName??adaptive
    //kTotalExecuted: ??????????????
    //kTasksQueued: ??????????????????task??
    //_deferredTasksQueued: ??????????????????deferredTask??
    //kThreadsInUse: ????????????task??????
    //kTotalQueued=kDeferredTasksQueued(deferred task)+kTasksQueued(????task)
    //kThreadsPending??????????????????????????????????????????????????????????????task????????
    //kThreadsRunning??????????????task????????????????????????????????????????????
	//kTotalTimeRunningUs:??????????????????????????????????????????
	//kTotalTimeExecutingUs????????????????????????????????????????(????????IO??????IO??????????)
	//kTotalTimeQueuedUs: ????????????????????????????????????????????????????????????????????


    //??????????worker??????????
    AtomicWord<int> _threadsRunning{0};
    //????????????????task??worker??????????????????????????
    AtomicWord<int> _threadsPending{0};
    //????????????task??????
    AtomicWord<int> _threadsInUse{0};
    //??????????????????????task????????????????????????task??(??????????dealTask)
    AtomicWord<int> _tasksQueued{0};
    //??????????????????deferredTask??,????????????????????deffered task??(??????????readTask)
    AtomicWord<int> _deferredTasksQueued{0};
    //TransportLayerManager::createWithConfig????????
    //??????????????
    TickTimer _lastScheduleTimer;
    		

		
	//??????????????????????????????????????????
    AtomicWord<TickSource::Tick> _pastThreadsSpentExecuting{0};
    //??????????????????????????????????????(????????IO??????IO??????????)
    AtomicWord<TickSource::Tick> _pastThreadsSpentRunning{0};
    //????????????????
    static thread_local ThreadState* _localThreadState;

    // These counters are only used for reporting in serverStatus.
    //??????????????
    AtomicWord<int64_t> _totalQueued{0};
    //??????????????
    AtomicWord<int64_t> _totalExecuted{0};
    //????????????????????????????????????????????????????????????????????
    AtomicWord<TickSource::Tick> _totalSpentQueued{0};

    // Threads signal this condition variable when they exit so we can gracefully shutdown
    // the executor.
    //shutdown????????????????????????????
    stdx::condition_variable _deathCondition;

    // Tasks should signal this condition variable if they want the thread controller to
    // track their progress and do fast stuck detection
    //??????????????????????????????????????????task????
    //????controler????,??????ServiceExecutorAdaptive::schedule????????_controllerThreadRoutine
    stdx::condition_variable _scheduleCondition;
};

}  // namespace transport
}  // namespace mongo
