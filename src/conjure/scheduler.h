#ifndef CONJURE_SCHEDULER_H_
#define CONJURE_SCHEDULER_H_

#include "conjure/conjury.h"
#include "conjure/log.h"
#include <assert.h>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

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

        enum ActionState { kReady, kSuspending, kIgnore };

        SuspendedConjury::ActionState ObserveState() {
            State s = c->GetState();
            if (s == State::kReady) {
                return ActionState::kReady;
            }
            if (s == State::kSuspended) {
                if (ready_pred and ready_pred()) {
                    c->UnsafeSetState(State::kReady);
                    return ActionState::kReady;
                }
                return ActionState::kSuspending;
            }
            return ActionState::kIgnore;
        }

        const char *Name() {
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
            } else {
                CONJURE_LOGF(
                    "ready_queue ignore: %s, state: %s", c->Name(),
                    state::ToString(c->GetState()));
            }
        }
        return nullptr;
    }

    void TryRefreshBlockingQueue() {
        assert(ready_queue_.empty());
        assert(not suspended_queue_.empty());
        int new_blocking_end = 0;
        for (int i = 0; i < suspended_queue_.size(); ++i) {
            auto &c = suspended_queue_[i];
            switch (c.ObserveState()) {
            case SuspendedConjury::kReady:
                RegisterReady(c.c);
                CONJURE_LOGF("ready from blocking_queue: %s", c.c->Name());
                break;
            case SuspendedConjury::kSuspending:
                suspended_queue_[new_blocking_end++] = std::move(c);
                break;
            default:
                CONJURE_LOGF(
                    "blocking_queue ignored: %s, state: %s", c.c->Name(),
                    state::ToString(c.c->GetState()));
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
