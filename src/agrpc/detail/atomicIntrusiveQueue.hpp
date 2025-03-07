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

#ifndef AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP
#define AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/intrusiveQueue.hpp"

#include <atomic>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
// Adapted from
// https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/atomic_intrusive_queue.hpp
template <class Item>
class AtomicIntrusiveQueue
{
  public:
    AtomicIntrusiveQueue() = default;

    explicit AtomicIntrusiveQueue(bool initially_active) noexcept
        : head(initially_active ? nullptr : this->producer_inactive_value())
    {
    }

    AtomicIntrusiveQueue(const AtomicIntrusiveQueue&) = delete;

    AtomicIntrusiveQueue(AtomicIntrusiveQueue&&) = delete;

    AtomicIntrusiveQueue& operator=(const AtomicIntrusiveQueue&) = delete;

    AtomicIntrusiveQueue& operator=(AtomicIntrusiveQueue&&) = delete;

    // Returns true if the previous state was inactive and this
    // operation successfully marked it as active.
    // Returns false if the previous state was active.
    [[nodiscard]] bool try_mark_active() noexcept
    {
        void* old_value = this->producer_inactive_value();
        return this->head.compare_exchange_strong(old_value, nullptr, std::memory_order_acquire,
                                                  std::memory_order_relaxed);
    }

    // Enqueue an item to the queue.
    //
    // Returns true if the producer is inactive and needs to be
    // woken up. The calling thread has responsibility for waking
    // up the producer.
    [[nodiscard]] bool enqueue(Item* item) noexcept
    {
        const void* const inactive = this->producer_inactive_value();
        void* old_value = this->head.load(std::memory_order_relaxed);
        do
        {
            item->next = (old_value == inactive) ? nullptr : static_cast<Item*>(old_value);
        } while (!this->head.compare_exchange_weak(old_value, item, std::memory_order_acq_rel));
        return old_value == inactive;
    }

    bool try_mark_inactive() noexcept
    {
        void* const inactive = this->producer_inactive_value();
        if (void* old_value = this->head.load(std::memory_order_relaxed); old_value == nullptr)
        {
            return this->head.compare_exchange_strong(old_value, inactive, std::memory_order_release,
                                                      std::memory_order_relaxed);
        }
        return false;
    }

    // Atomically either mark the producer as inactive if the queue was empty
    // or dequeue pending items from the queue.
    //
    // Not valid to call if the producer is already marked as inactive.
    [[nodiscard]] detail::IntrusiveQueue<Item> try_mark_inactive_or_dequeue_all() noexcept
    {
        if (this->try_mark_inactive())
        {
            return {};
        }
        void* const old_value = this->head.exchange(nullptr, std::memory_order_acquire);
        return detail::IntrusiveQueue<Item>::make_reversed(static_cast<Item*>(old_value));
    }

  private:
    [[nodiscard]] void* producer_inactive_value() const noexcept
    {
        // Pick some pointer that is not nullptr and that is
        // guaranteed to not be the address of a valid item.
        return const_cast<void*>(static_cast<const void*>(&this->head));
    }

    std::atomic<void*> head{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP
