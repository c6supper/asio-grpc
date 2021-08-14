// Copyright 2021 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AGRPC_DETAIL_RPCS_HPP
#define AGRPC_DETAIL_RPCS_HPP

#include "agrpc/detail/asioForward.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#include <utility>

namespace agrpc::detail
{
template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class RPC, class Request, class Reader>
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                     void*);

template <class RPC, class Writer, class Response>
using ClientSideStreamingRequest = Writer (RPC::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class Deadline>
struct AlarmFunction
{
    grpc::Alarm& alarm;
    Deadline deadline;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        alarm.Set(grpc_context.get_completion_queue(), deadline, tag);
    }
};

template <class Deadline>
AlarmFunction(grpc::Alarm&, const Deadline&) -> AlarmFunction<Deadline>;

template <class RPC, class Service, class Request, class Responder>
struct ServerMultiArgRequestFunction
{
    detail::ServerMultiArgRequest<RPC, Request, Responder> rpc;
    Service& service;
    grpc::ServerContext& server_context;
    Request& request;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &request, &responder, cq, cq, tag);
    }
};

template <class RPC, class Service, class Request, class Responder>
ServerMultiArgRequestFunction(detail::ServerMultiArgRequest<RPC, Request, Responder>, Service&, grpc::ServerContext&,
                              Request&, Responder&) -> ServerMultiArgRequestFunction<RPC, Service, Request, Responder>;

template <class RPC, class Service, class Responder>
struct ServerSingleArgRequestFunction
{
    detail::ServerSingleArgRequest<RPC, Responder> rpc;
    Service& service;
    grpc::ServerContext& server_context;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &responder, cq, cq, tag);
    }
};

template <class RPC, class Service, class Responder>
ServerSingleArgRequestFunction(detail::ServerSingleArgRequest<RPC, Responder>, Service&, grpc::ServerContext&,
                               Responder&) -> ServerSingleArgRequestFunction<RPC, Service, Responder>;

template <class Response, class Request>
struct ServerAsyncReaderFunctions
{
    using Responder = grpc::ServerAsyncReader<Response, Request>;

    struct Read
    {
        Responder& responder;
        Request& request;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Read(&request, tag); }
    };

    struct Finish
    {
        Responder& responder;
        const Response& response;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Finish(response, status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Response>
struct ServerAsyncWriterFunctions
{
    using Responder = grpc::ServerAsyncWriter<Response>;

    struct Write
    {
        Responder& responder;
        const Response& response;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Write(response, tag); }
    };

    struct Finish
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Finish(status, tag); }
    };
};

template <class Response>
struct ServerAsyncResponseWriterFunctions
{
    using Responder = grpc::ServerAsyncResponseWriter<Response>;

    struct Write
    {
        Responder& responder;
        const Response& response;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Finish(response, status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Response, class Request>
struct ServerAsyncReaderWriterFunctions
{
    using Responder = grpc::ServerAsyncReaderWriter<Response, Request>;

    struct Read
    {
        Responder& responder;
        Request& request;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Read(&request, tag); }
    };

    struct Write
    {
        Responder& responder;
        const Response& response;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Write(response, tag); }
    };

    struct WriteAndFinish
    {
        Responder& responder;
        const Response& response;
        grpc::WriteOptions options;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.WriteAndFinish(response, options, status, tag); }
    };

    struct Finish
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(agrpc::GrpcContext&, void* tag) { responder.Finish(status, tag); }
    };
};

template <class Responder, class CompletionHandler>
struct CompletionHandlerWithResponder
{
    using executor_type = typename asio::associated_executor<CompletionHandler>::type;

    CompletionHandler completion_handler;
    Responder responder;

    template <class... Args>
    CompletionHandlerWithResponder(CompletionHandler completion_handler, Args&&... args)
        : completion_handler(std::move(completion_handler)), responder(std::forward<Args>(args)...)
    {
    }

    void operator()(bool ok) { this->completion_handler(std::pair{std::move(this->responder), ok}); }

    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler);
    }
};

template <class Responder, class CompletionHandler, class... Args>
auto make_completion_handler_with_responder(CompletionHandler completion_handler, Args&&... args)
{
    return detail::CompletionHandlerWithResponder<Responder, CompletionHandler>{std::move(completion_handler),
                                                                                std::forward<Args>(args)...};
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_RPCS_HPP
