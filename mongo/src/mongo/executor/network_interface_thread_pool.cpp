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

#define MONGO_LOG_DEFAULT_COMPONENT mongo::logger::LogComponent::kExecutor

#include "mongo/platform/basic.h"

#include "mongo/executor/network_interface_thread_pool.h"

#include "mongo/executor/network_interface.h"
#include "mongo/util/destructor_guard.h"
#include "mongo/util/log.h"
#include "mongo/util/scopeguard.h"

namespace mongo {
namespace executor {

NetworkInterfaceThreadPool::NetworkInterfaceThreadPool(NetworkInterface* net) : _net(net) {}

NetworkInterfaceThreadPool::~NetworkInterfaceThreadPool() {
    DESTRUCTOR_GUARD(dtorImpl());
}

void NetworkInterfaceThreadPool::dtorImpl() {
    {
        stdx::unique_lock<stdx::mutex> lk(_mutex);

        if (_tasks.empty())
            return;

        _inShutdown = true;
    }

    join();

    invariant(_tasks.empty());
}

//ThreadPoolTaskExecutor::startup
void NetworkInterfaceThreadPool::startup() {
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    if (_started) {
        severe() << "Attempting to start pool, but it has already started";
        fassertFailed(34358);
    }
    _started = true;

    consumeTasks(std::move(lk));
}

void NetworkInterfaceThreadPool::shutdown() {
    {
        stdx::lock_guard<stdx::mutex> lk(_mutex);
        _inShutdown = true;
    }

    _net->signalWorkAvailable();
}

void NetworkInterfaceThreadPool::join() {
    {
        stdx::unique_lock<stdx::mutex> lk(_mutex);

        if (_joining) {
            severe() << "Attempted to join pool more than once";
            fassertFailed(34357);
        }

        _joining = true;
        _started = true;

        consumeTasks(std::move(lk));
    }

    _net->signalWorkAvailable();

    stdx::unique_lock<stdx::mutex> lk(_mutex);
    _joiningCondition.wait(lk, [&] { return _tasks.empty() && (!_consumingTasks); });
}

//task入队到_tasks
//ThreadPoolTaskExecutor::scheduleIntoPool_inlock调用，后端mongod应答后都会走到这里，等待NetworkInterfaceThreadPool::consumeTasks task消费
Status NetworkInterfaceThreadPool::schedule(Task task) { //task对应remoteCommandFinished，参考ThreadPoolTaskExecutor::scheduleRemoteCommand
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    if (_inShutdown) {
        return {ErrorCodes::ShutdownInProgress, "Shutdown in progress"};
    }
    _tasks.emplace_back(std::move(task));

    if (_started)
        consumeTasks(std::move(lk));

    return Status::OK();
}

/**
 * Consumes available tasks.
 *
 * We distinguish between calls to consume on the networking thread and off of
 * it. For off thread calls, we try to initiate a consume via setAlarm, while on
 * it we invoke directly. This allows us to use the network interface's threads
 * as our own pool, which should reduce context switches if our tasks are
 * getting scheduled by network interface tasks.
 */ //消费task执行 NetworkInterfaceThreadPool::consumeTasks  生成task在NetworkInterfaceThreadPool::schedule
 //NetworkInterfaceThreadPool::startup中调用该函数消费task
void NetworkInterfaceThreadPool::consumeTasks(stdx::unique_lock<stdx::mutex> lk) {
    if (_consumingTasks || _tasks.empty())
        return;

    if (!(_inShutdown || _net->onNetworkThread())) {
        if (!_registeredAlarm) {
            _registeredAlarm = true;
            lk.unlock();
            _net->setAlarm(_net->now(),
                           [this] {
                               stdx::unique_lock<stdx::mutex> lk(_mutex);
                               _registeredAlarm = false;
                               consumeTasks(std::move(lk));
                           })
                .transitional_ignore();
        }

        return;
    }

    _consumingTasks = true;
    const auto consumingTasksGuard = MakeGuard([&] { _consumingTasks = false; });

    decltype(_tasks) tasks;

	//注意以下线程执行该task
	//2019-03-10T19:03:37.339+0800 F EXECUTOR [ReplicaSetMonitor-TaskExecutor-0] ddd test ... NetworkInterfaceThreadPool::consumeTasks
	//2019-03-10T19:03:37.339+0800 F EXECUTOR [ShardRegistryUpdater-0] ddd test ... NetworkInterfaceThreadPool::consumeTasks
	//2019-03-10T19:03:37.346+0800 F EXECUTOR [NetworkInterfaceASIO-ShardRegistry-0] ddd test ... NetworkInterfaceThreadPool::consumeTasks
	//severe() << "ddd test ... NetworkInterfaceThreadPool::consumeTasks";	
    while (_tasks.size()) {
        using std::swap;
        swap(tasks, _tasks);

        lk.unlock();
        const auto lkGuard = MakeGuard([&] { lk.lock(); });

        for (auto&& task : tasks) {
            try {
                task();
            } catch (...) {
                severe() << "Exception escaped task in network interface thread pool";
                std::terminate();
            }
        }

        tasks.clear();
    }

    if (_joining)
        _joiningCondition.notify_one();
}

}  // namespace executor
}  // namespace mongo
