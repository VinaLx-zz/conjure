#include "conjure/interfaces.h"

namespace conjure {

void Scheduler::Run(Scheduler *sche) {
    for (;;) {
        YieldFromReadyQueue(*sche);
        sche->UseBakSuspendedQueue();
        YieldFromSuspendedQueue(*sche);
        sche->UseMajorSuspendedQueue();
    }
}

void Scheduler::UseBakSuspendedQueue() {
    current_suspended_queue_ = &bak_suspended_queue_;
}

void Scheduler::UseMajorSuspendedQueue() {
    if (bak_suspended_queue_.size() > 0) {
        CONJURE_LOGF(
            "flush %d suspended into major queue",
            (int)bak_suspended_queue_.size());
    }
    for (auto &sc : bak_suspended_queue_) {
        suspended_queue_.push_back(std::move(sc));
    }
    bak_suspended_queue_.clear();
    current_suspended_queue_ = &suspended_queue_;
}

void Scheduler::YieldTo(Conjury *conjury) {
    conjurer_->stage_.UnsafeSwitchTo(conjury);
}

Scheduler::SuspendedConjury::ActionState
Scheduler::SuspendedConjury::ObserveState() {
    c->ConsumeWakeUp();
    State s = c->GetState();
    if (not ready_pred) {
        printf("%s ???\n", c->Name());
        exit(1);
    }
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

void Scheduler::YieldFromReadyQueue(Scheduler &sche) {
    for (; not sche.ready_queue_.empty();) {
        Conjury *c = sche.ready_queue_.front();
        sche.ready_queue_.pop_front();
        if (c->GetState() == State::kReady) {
            sche.YieldTo(c);
        } else {
            CONJURE_LOGF(
                "ready_queue ignore: %s, state: %s", c->Name(),
                state::ToString(c->GetState()));
        }
    }
}

void Scheduler::YieldFromSuspendedQueue(Scheduler &sche) {
    assert(sche.ready_queue_.empty());
    assert(not sche.suspended_queue_.empty());
    int new_blocking_end = 0;
    for (int i = 0; i < sche.suspended_queue_.size(); ++i) {
        auto &c = sche.suspended_queue_[i];
        switch (c.ObserveState()) {
        case SuspendedConjury::kReady:
            CONJURE_LOGF("ready from blocking_queue: %s", c.c->Name());
            CONJURE_LOGF(
                "index: %d, total suspended: %lu, delay queue: %d", i,
                sche.suspended_queue_.size(),
                (int)sche.bak_suspended_queue_.size());
            sche.YieldTo(c.c);
            break;
        case SuspendedConjury::kSuspending:
            if (i != new_blocking_end++) {
                sche.suspended_queue_[new_blocking_end - 1] = std::move(c);
            }
            break;
        default:
            CONJURE_LOGF(
                "blocking_queue ignored: %s, state: %s", c.c->Name(),
                state::ToString(c.c->GetState()));
        }
    }
    sche.suspended_queue_.erase(
        begin(sche.suspended_queue_) + new_blocking_end,
        end(sche.suspended_queue_));
}

} // namespace conjure
