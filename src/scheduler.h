#ifndef CONJURE_SCHEDULER_H_
#define CONJURE_SCHEDULER_H_

#include "./conjury.h"
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <stdio.h>

namespace conjure {

class Scheduler {
  public:
    using Pointer = std::unique_ptr<Scheduler>;

    void RegisterReady(Conjury *c) {
        ready_queue_.push_back(c);
    }

    void RegisterSuspended(Conjury *c) {
        suspended_queue_.emplace_back(c);
    }

    template <typename P>
    void RegisterSuspended(Conjury *c, P p) {
        suspended_queue_.emplace_back(c, std::move(p));
    }

    Conjury *GetNext() {
        for (;;) {
            if (Conjury *c = TryGetReadyQueueNext()) {
                return c;
            }
            TryRefreshBlockingQueue();
        }
        return nullptr;
    }

  private:
    struct SuspendedConjury {
        SuspendedConjury(Conjury *c) : c(c) {}

        template <typename P>
        SuspendedConjury(Conjury *c, P p) : c(c), ready_pred(std::move(p)) {}

        bool IsReady() const {
            return c->GetState() == State::kReady or
                   (c->GetState() == State::kSuspended and ready_pred and
                    ready_pred());
        }

        const std::string &Name() {
            return c->Name();
        }

        Conjury *c;
        std::function<bool()> ready_pred;
    };

    Conjury *TryGetReadyQueueNext() {
        for (; not ready_queue_.empty();) {
            Conjury *c = ready_queue_.front();
            ready_queue_.pop_front();
            if (c->GetState() == State::kReady) {
                return c;
            }
        }
        return nullptr;
    }

    void TryRefreshBlockingQueue() {
        int new_blocking_end = 0;
        for (int i = 0; i < suspended_queue_.size(); ++i) {
            auto &c = suspended_queue_[i];
            if (c.IsReady()) {
                c.c->UnsafeSetState(State::kReady);
                RegisterReady(c.c);
            } else if (c.c->GetState() == State::kSuspended) {
                suspended_queue_[new_blocking_end++] = std::move(c);
            }
        }
        suspended_queue_.erase(
            begin(suspended_queue_) + new_blocking_end, end(suspended_queue_));
    }

    std::deque<Conjury *> ready_queue_;
    std::vector<SuspendedConjury> suspended_queue_;
};

} // namespace conjure

#endif
