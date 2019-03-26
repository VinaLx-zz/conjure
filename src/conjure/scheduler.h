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

class Conjurer;

class Scheduler {
  public:
    Scheduler(Conjurer *conjurer)
        : conjurer_(conjurer), current_suspended_queue_(&suspended_queue_) {}

    using Pointer = std::unique_ptr<Scheduler>;

    static void Run(Scheduler *sche);

    void RegisterReady(Conjury *c) {
        ready_queue_.push_back(c);
    }

    void RegisterSuspended(Conjury *c) {
        suspended_queue_.emplace_back(c);
    }

    template <typename P>
    void RegisterSuspended(Conjury *c, P p) {
        current_suspended_queue_->emplace_back(c, std::move(p));
    }

  private:
    struct SuspendedConjury {
        SuspendedConjury(Conjury *c) : c(c) {}

        template <typename P>
        SuspendedConjury(Conjury *c, P p) : c(c), ready_pred(std::move(p)) {}

        enum ActionState { kReady, kSuspending, kIgnore };

        SuspendedConjury::ActionState ObserveState();

        const char *Name() {
            return c->Name();
        }

        Conjury *c;
        std::function<bool()> ready_pred;
    };

    static void YieldFromReadyQueue(Scheduler &sche);

    static void YieldFromSuspendedQueue(Scheduler &sche);

    void YieldTo(Conjury *conjury);

    void UseBakSuspendedQueue(); 

    void UseMajorSuspendedQueue();

    Conjurer *conjurer_;

    std::deque<Conjury *> ready_queue_;
    std::vector<SuspendedConjury> suspended_queue_;
    std::vector<SuspendedConjury> bak_suspended_queue_;

    std::vector<SuspendedConjury> *current_suspended_queue_;
};

} // namespace conjure

#endif
