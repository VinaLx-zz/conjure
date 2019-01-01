#ifndef CONJURE_SCHEDULER_H_
#define CONJURE_SCHEDULER_H_

#include "./conjury.h"
#include <deque>
#include <memory>
#include <stdexcept>
#include <stdio.h>

namespace conjure {

class Scheduler {
  public:
    using Pointer = std::unique_ptr<Scheduler>;

    void RegisterReady(Conjury* c) {
        ready_queue_.push_back(c);
    }

    void RegisterSuspended(Conjury* c) {
        blocking_queue_.push_back(c);
    }

    Conjury* GetNext() {
        // printf("calling getNext\n");
        for (;;) {
            if (Conjury* c = TryGetReadyQueueNext()) {
                return c;
            }
            TryRefreshBlockingQueue();
        }
        return nullptr;
    }

  private:
    Conjury* TryGetReadyQueueNext() {
        for (; not ready_queue_.empty(); ) {
            Conjury* c = ready_queue_.front();
            ready_queue_.pop_front();
            if (c->GetState() == Conjury::State::kReady) {
                return c;
            }
        }
        return nullptr;
    }

    void TryRefreshBlockingQueue() {
        int new_blocking_end = 0;
        for (int i = 0; i < blocking_queue_.size(); ++i) {
            Conjury* c = blocking_queue_[i];
            switch (c->GetState()) {
                case Conjury::State::kBlocking:
                    blocking_queue_[new_blocking_end++] = c;
                    break;
                case Conjury::State::kReady:
                    RegisterReady(c);
                    break;
                case Conjury::State::kFinished:
                    break;
                default:
                    throw std::logic_error("invalid conjury state");
            }
        }
        blocking_queue_.erase(
                begin(blocking_queue_) + new_blocking_end,
                end(blocking_queue_));
    }

    std::deque<Conjury*> ready_queue_;
    std::vector<Conjury*> blocking_queue_;
};

}  // namespace conjure

#endif
