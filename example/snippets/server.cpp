// Copyright 2022 Dennis Hezel
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

#include "example/v1/example.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <chrono>

namespace asio = boost::asio;

asio::awaitable<void> timer()
{
    /* [alarm-awaitable] */
    grpc::Alarm alarm;
    bool wait_ok =
        co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), asio::use_awaitable);
    /* [alarm-awaitable] */

    silence_unused(wait_ok);
}

asio::awaitable<void> timer_with_different_completion_tokens(agrpc::GrpcContext& grpc_context)
{
    std::allocator<std::byte> my_allocator{};
    grpc::Alarm alarm;
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
    /* [alarm-with-callback] */
    agrpc::wait(alarm, deadline, asio::bind_executor(grpc_context, [&](bool /*wait_ok*/) {}));
    /* [alarm-with-callback] */

    /* [alarm-with-allocator-aware-awaitable] */
    co_await agrpc::wait(alarm, deadline, agrpc::bind_allocator(my_allocator, asio::use_awaitable));
    /* [alarm-with-allocator-aware-awaitable] */

    /* [alarm-with-allocator-aware-executor] */
    agrpc::wait(
        alarm, deadline,
        asio::bind_executor(asio::require(grpc_context.get_executor(), asio::execution::allocator(my_allocator)),
                            [&](bool /*wait_ok*/) {}));
    /* [alarm-with-allocator-aware-executor] */

    /* [alarm-stackless-coroutine] */
    struct Coro : asio::coroutine
    {
        using executor_type = agrpc::GrpcContext::executor_type;

        struct Context
        {
            std::chrono::system_clock::time_point deadline;
            agrpc::GrpcContext& grpc_context;
            grpc::Alarm alarm;

            Context(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
                : deadline(deadline), grpc_context(grpc_context)
            {
            }
        };

        std::shared_ptr<Context> context;

        Coro(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
            : context(std::make_shared<Context>(deadline, grpc_context))
        {
        }

        void operator()(bool wait_ok)
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                BOOST_ASIO_CORO_YIELD agrpc::wait(context->alarm, context->deadline, std::move(*this));
                (void)wait_ok;
            }
        }

        executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
    };
    Coro{deadline, grpc_context}(false);
    /* [alarm-stackless-coroutine] */
}

asio::awaitable<void> unary(example::v1::Example::AsyncService& service)
{
    /* [request-unary-server-side] */
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service,
                                              server_context, request, writer, asio::use_awaitable);
    /* [request-unary-server-side] */

    /* [send_initial_metadata-unary-server-side] */
    bool send_ok = co_await agrpc::send_initial_metadata(writer, asio::use_awaitable);
    /* [send_initial_metadata-unary-server-side] */

    /* [finish-unary-server-side] */
    example::v1::Response response;
    bool finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK, asio::use_awaitable);
    /* [finish-unary-server-side] */

    /* [finish_with_error-unary-server-side] */
    bool finish_with_error_ok = co_await agrpc::finish_with_error(writer, grpc::Status::CANCELLED, asio::use_awaitable);
    /* [finish_with_error-unary-server-side] */

    silence_unused(request_ok, send_ok, finish_ok, finish_with_error_ok);
}

asio::awaitable<void> client_streaming(example::v1::Example::AsyncService& service)
{
    /* [request-client-streaming-server-side] */
    grpc::ServerContext server_context;
    grpc::ServerAsyncReader<example::v1::Response, example::v1::Request> reader{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                                              server_context, reader, asio::use_awaitable);
    /* [request-client-streaming-server-side] */

    /* [read-client-streaming-server-side] */
    example::v1::Request request;
    bool read_ok = co_await agrpc::read(reader, request, asio::use_awaitable);
    /* [read-client-streaming-server-side] */

    /* [finish-client-streaming-server-side] */
    example::v1::Response response;
    bool finish_ok = co_await agrpc::finish(reader, response, grpc::Status::OK, asio::use_awaitable);
    /* [finish-client-streaming-server-side] */

    /* [finish_with_error-client-streaming-server-side] */
    bool finish_with_error_ok = co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED, asio::use_awaitable);
    /* [finish_with_error-client-streaming-server-side] */

    silence_unused(request_ok, read_ok, finish_with_error_ok, finish_ok);
}

asio::awaitable<void> server_streaming(example::v1::Example::AsyncService& service)
{
    /* [request-server-streaming-server-side] */
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncWriter<example::v1::Response> writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestServerStreaming, service,
                                              server_context, request, writer, asio::use_awaitable);
    /* [request-server-streaming-server-side] */

    /* [write-server-streaming-server-side] */
    example::v1::Response response;
    bool write_ok = co_await agrpc::write(writer, response, asio::use_awaitable);
    /* [write-server-streaming-server-side] */

    /* [write_last-server-streaming-server-side] */
    bool write_last_ok = co_await agrpc::write_last(writer, response, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-server-streaming-server-side] */

    /* [write_and_finish-server-streaming-server-side] */
    bool write_and_finish_ok =
        co_await agrpc::write_and_finish(writer, response, grpc::WriteOptions{}, grpc::Status::OK, asio::use_awaitable);
    /* [write_and_finish-server-streaming-server-side] */

    /* [finish-server-streaming-server-side] */
    bool finish_ok = co_await agrpc::finish(writer, grpc::Status::OK, asio::use_awaitable);
    /* [finish-server-streaming-server-side] */

    silence_unused(request_ok, write_ok, write_last_ok, write_and_finish_ok, finish_ok);
}

asio::awaitable<void> bidirectional_streaming(example::v1::Example::AsyncService& service)
{
    /* [request-bidirectional-streaming-server-side] */
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming,
                                              service, server_context, reader_writer, asio::use_awaitable);
    /* [request-bidirectional-streaming-server-side] */

    /* [read-bidirectional-streaming-server-side] */
    example::v1::Request request;
    bool read_ok = co_await agrpc::read(reader_writer, request, asio::use_awaitable);
    /* [read-bidirectional-streaming-server-side] */

    /* [write_last-bidirectional-streaming-server-side] */
    example::v1::Response response;
    bool write_last_ok = co_await agrpc::write_last(reader_writer, response, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-bidirectional-streaming-server-side] */

    /* [write_and_finish-bidirectional-streaming-server-side] */
    bool write_and_finish_ok = co_await agrpc::write_and_finish(reader_writer, response, grpc::WriteOptions{},
                                                                grpc::Status::OK, asio::use_awaitable);
    /* [write_and_finish-bidirectional-streaming-server-side] */

    /* [write-bidirectional-streaming-server-side] */
    bool write_ok = co_await agrpc::write(reader_writer, response, asio::use_awaitable);
    /* [write-bidirectional-streaming-server-side] */

    /* [finish-bidirectional-streaming-server-side] */
    bool finish_ok = co_await agrpc::finish(reader_writer, grpc::Status::OK, asio::use_awaitable);
    /* [finish-bidirectional-streaming-server-side] */

    silence_unused(request_ok, read_ok, write_last_ok, write_and_finish_ok, write_ok, finish_ok);
}

void io_context(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    /* [bind-executor-to-use-awaitable] */
    asio::io_context io_context;
    asio::co_spawn(
        io_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ServerContext server_context;
            grpc::ServerAsyncReader<example::v1::Response, example::v1::Request> reader{&server_context};
            // error: asio::this_coro::executor does not refer to a GrpcContext
            // co_await agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
            //                        server_context, reader, asio::use_awaitable);

            // correct:
            co_await agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                                    server_context, reader, asio::bind_executor(grpc_context, asio::use_awaitable));
        },
        asio::detached);
    /* [bind-executor-to-use-awaitable] */
}

/* [repeatedly-request-callback] */
template <class Executor, class Handler>
struct AssociatedHandler
{
    using executor_type = Executor;

    Executor executor;
    Handler handler;

    AssociatedHandler(Executor executor, Handler handler) : executor(std::move(executor)), handler(std::move(handler))
    {
    }

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& request_context)
    {
        std::invoke(handler, std::move(request_context), executor);
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }
};

void repeatedly_request_example(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        AssociatedHandler{
            asio::require(grpc_context.get_executor(), asio::execution::allocator(grpc_context.get_allocator())),
            [](auto&& request_context, auto&& executor)
            {
                auto& writer = request_context.responder();
                example::v1::Response response;
                agrpc::finish(writer, response, grpc::Status::OK,
                              asio::bind_executor(executor, [c = std::move(request_context)](bool) {}));
            }});
}
/* [repeatedly-request-callback] */

/* [repeatedly-request-awaitable] */
void register_client_streaming_handler(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext&,
                grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>&) -> asio::awaitable<void>
            {
                // ...
                co_return;
            }));
}
/* [repeatedly-request-awaitable] */

void create_grpc_context()
{
    /* [create-grpc_context-server-side] */
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    /* [create-grpc_context-server-side] */
}

int main()
{
    std::unique_ptr<grpc::Server> server;
    example::v1::Example::AsyncService service;

    // begin-snippet: create-grpc_context-server-side
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    // end-snippet

    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    auto guard = asio::make_work_guard(grpc_context);
    asio::co_spawn(
        grpc_context,
        [&]()
        {
            return unary(service);
        },
        asio::detached);

    // begin-snippet: run-grpc_context-server-side
    grpc_context.run();
    server->Shutdown();
}  // grpc_context is destructed here before the server
   // end-snippet