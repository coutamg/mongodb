/**
 *    Copyright (C) 2016 MongoDB Inc.
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

#include "mongo/platform/basic.h"

#include "mongo/transport/transport_layer_manager.h"

#include "mongo/base/status.h"
#include "mongo/db/server_options.h"
#include "mongo/db/service_context.h"
#include "mongo/stdx/memory.h"
#include "mongo/transport/service_executor_adaptive.h"
#include "mongo/transport/service_executor_synchronous.h"
#include "mongo/transport/session.h"
#include "mongo/transport/transport_layer_asio.h"
#include "mongo/transport/transport_layer_legacy.h"
#include "mongo/util/net/ssl_types.h"
#include "mongo/util/time_support.h"
#include <limits>

#include <iostream>

namespace mongo {
namespace transport {

TransportLayerManager::TransportLayerManager() = default;

Ticket TransportLayerManager::sourceMessage(const SessionHandle& session,
                                            Message* message,
                                            Date_t expiration) {
	//TransportLayerASIO::sourceMessage
	return session->getTransportLayer()->sourceMessage(session, message, expiration);
}

Ticket TransportLayerManager::sinkMessage(const SessionHandle& session,
                                          const Message& message,
                                          Date_t expiration) {
    //TransportLayerASIO::sinkMessage
    return session->getTransportLayer()->sinkMessage(session, message, expiration);
}

Status TransportLayerManager::wait(Ticket&& ticket) {
    return getTicketTransportLayer(ticket)->wait(std::move(ticket));
}

void TransportLayerManager::asyncWait(Ticket&& ticket, TicketCallback callback) {
    return getTicketTransportLayer(ticket)->asyncWait(std::move(ticket), std::move(callback));
}

template <typename Callable>
	//TransportLayerManager::shutdown??????????tls??????cb
void TransportLayerManager::_foreach(Callable&& cb) const {
    {
        stdx::lock_guard<stdx::mutex> lk(_tlsMutex);
        for (auto&& tl : _tls) {
            cb(tl.get());
        }
    }
}

void TransportLayerManager::end(const SessionHandle& session) {
    session->getTransportLayer()->end(session);
}

// TODO Right now this and setup() leave TLs started if there's an error. In practice the server
// exits with an error and this isn't an issue, but we should make this more robust.
//TransportLayerASIO::start  accept????
//TransportLayerASIO::setup() listen????
//??????accept??????_initAndListen->TransportLayerASIO::start????????????????????????
Status TransportLayerManager::start() {
    for (auto&& tl : _tls) {
		//TransportLayerASIO::start??????accept??????????
        auto status = tl->start();  
        if (!status.isOK()) {
            _tls.clear();
            return status;
        }
    }

    return Status::OK();
}

void TransportLayerManager::shutdown() {
	//TransportLayerASIO::shutdown????????????????
    _foreach([](TransportLayer* tl) { tl->shutdown(); });
}

//TransportLayerASIO::start  accept????
//TransportLayerASIO::setup() listen????
// TODO Same comment as start() 

//??????accept??????_initAndListen->TransportLayerASIO::start????????????????????????
//runMongosServer _initAndListen??????
Status TransportLayerManager::setup() {
    //_tls??????TransportLayerManager::createWithConfig??????retVector
    for (auto&& tl : _tls) {
		//TransportLayerASIO::setup() listen????
        auto status = tl->setup(); 
        if (!status.isOK()) {
            _tls.clear();
            return status;
        }
    }

    return Status::OK();
}

//????????????????????????TransportLayerManager::start
Status TransportLayerManager::addAndStartTransportLayer(std::unique_ptr<TransportLayer> tl) {
    auto ptr = tl.get();
    {
        stdx::lock_guard<stdx::mutex> lk(_tlsMutex);
        _tls.emplace_back(std::move(tl));
    }
	//TransportLayerASIO::start
    return ptr->start();
}

//??????????????????????  _initAndListen??????
std::unique_ptr<TransportLayer> TransportLayerManager::createWithConfig(
    const ServerGlobalParams* config, ServiceContext* ctx) {
    std::unique_ptr<TransportLayer> transportLayer;
	//????????????????????????mongos????mongod
	//mongos????ServiceEntryPointMongod,mongod????ServiceEntryPointMongos
    auto sep = ctx->getServiceEntryPoint();
	//net.transportLayer??????????????asio, legacy??????????
    if (config->transportLayer == "asio") {
		//????asio??????????????????
        transport::TransportLayerASIO::Options opts(config);

		//??????????????????????????synchronous
        if (config->serviceExecutor == "adaptive") {
			//??????????????,??????????????
            opts.transportMode = transport::Mode::kAsynchronous;
        } else if (config->serviceExecutor == "synchronous") {
            //????????????????????????????????????
            opts.transportMode = transport::Mode::kSynchronous;
        } else {
            MONGO_UNREACHABLE;
        }

		//??????????asio,????TransportLayerASIO??
        auto transportLayerASIO = stdx::make_unique<transport::TransportLayerASIO>(opts, sep);

		//ServiceExecutorSynchronous????????????????????ServiceExecutorAdaptive????????????????????????
		if (config->serviceExecutor == "adaptive") { //????????
			//????????????????????????????ServiceExecutorAdaptive
            ctx->setServiceExecutor(stdx::make_unique<ServiceExecutorAdaptive>(
                ctx, transportLayerASIO->getIOContext()));
        } else if (config->serviceExecutor == "synchronous") { //????????
        	//????????????????????????????????????ServiceExecutorSynchronous
            ctx->setServiceExecutor(stdx::make_unique<ServiceExecutorSynchronous>(ctx));
        }
		//transportLayerASIO??????transportLayer??
        transportLayer = std::move(transportLayerASIO);
    } else if (serverGlobalParams.transportLayer == "legacy") {
		//????legacy????????????????????????transportLayer
		transport::TransportLayerLegacy::Options opts(config);
        transportLayer = stdx::make_unique<transport::TransportLayerLegacy>(opts, sep);
        ctx->setServiceExecutor(stdx::make_unique<ServiceExecutorSynchronous>(ctx));
    }

	//transportLayer??????????retVector????????????
    std::vector<std::unique_ptr<TransportLayer>> retVector;
    retVector.emplace_back(std::move(transportLayer));
	//????TransportLayerManager??????????????????retVector??????_tls????????TransportLayerManager????????
    return stdx::make_unique<TransportLayerManager>(std::move(retVector));
}

}  // namespace transport
}  // namespace mongo

