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

#ifndef AGRPC_AGRPC_SCHEDULESENDER_HPP
#define AGRPC_AGRPC_SCHEDULESENDER_HPP

#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/receiver.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc
{
struct ScheduleSender
{
  private:
    template <class Receiver>
    class Operation : private detail::TypeErasedNoArgOperation
    {
      public:
        template <class Receiver2>
        constexpr explicit Operation(const ScheduleSender& sender, Receiver2&& receiver)
            : detail::TypeErasedNoArgOperation(&Operation::on_complete),
              impl(sender.grpc_context, std::forward<Receiver2>(receiver))
        {
        }

        void start() & noexcept
        {
            if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context()))
            {
                detail::GrpcContextImplementation::add_local_operation(grpc_context(), this);
            }
            else
            {
                detail::GrpcContextImplementation::add_remote_operation(grpc_context(), this);
            }
        }

      private:
        static void on_complete(detail::TypeErasedNoArgOperation* op, detail::InvokeHandler,
                                detail::GrpcContextLocalAllocator) noexcept
        {
            auto& self = *static_cast<Operation*>(op);
            detail::satisfy_receiver(std::move(self.receiver()));
        }

        constexpr decltype(auto) grpc_context() noexcept { return impl.first(); }

        constexpr decltype(auto) receiver() noexcept { return impl.second(); }

        detail::CompressedPair<agrpc::GrpcContext&, Receiver> impl;
    };

  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<>>;

    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    static constexpr bool sends_done = false;

    constexpr explicit ScheduleSender(agrpc::GrpcContext& grpc_context) noexcept : grpc_context(grpc_context) {}

    template <class Receiver>
    constexpr Operation<detail::RemoveCvrefT<Receiver>> connect(Receiver&& receiver) const
        noexcept(std::is_nothrow_constructible_v<Receiver, Receiver&&>)
    {
        return Operation<detail::RemoveCvrefT<Receiver>>{*this, std::forward<Receiver>(receiver)};
    }

  private:
    agrpc::GrpcContext& grpc_context;
};
}  // namespace agrpc

#endif  // AGRPC_AGRPC_SCHEDULESENDER_HPP
