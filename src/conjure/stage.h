#ifndef CONJURE_STAGE_H_
#define CONJURE_STAGE_H_

#include "conjure/conjury.h"
#include <algorithm>
#include <vector>

namespace conjure {

class Stage {
  public:
    Stage(Conjury::Pointer main_co) : active_conjury_(main_co.get()) {
        conjuries_.push_back(std::move(main_co));
    }

    template <typename S = Void>
    void UnsafeSwitchTo(Conjury *to, S state = S{}) {
        Conjury *current = active_conjury_;
        active_conjury_ = to;
        Stage::SetState(current, state);
        current->SwitchTo(*to);
    }

    template <typename S = Void>
    bool SwitchTo(Conjury *to, S state = S{}) {
        State to_state = to->GetState();
        if (to_state == State::kFinished) {
            return true;
        }
        if (not state::IsExecutable(to_state)) {
            CONJURE_LOGF(
                "conjury %s not executable with state: %s", to->Name(),
                state::ToString(to_state));
            return false;
        }
        UnsafeSwitchTo(to, state);
        return true;
    }

    Conjury *ActiveConjury() {
        return active_conjury_;
    }

    void Manage(Conjury::Pointer co) {
        conjuries_.push_back(std::move(co));
    }

    bool Destroy(const Conjury *co) {
        auto iter = std::find_if(
            begin(conjuries_), end(conjuries_),
            [co](const auto &cop) { return cop.get() == co; });
        if (iter == end(conjuries_)) {
            return false;
        }
        conjuries_.erase(iter);
        return true;
    }

  private:
    static void SetState(Conjury *conjury, State s) {
        conjury->UnsafeSetState(s);
    }
    static void SetState(Conjury *conjury, Void) {
        // do nothing
    }

    Conjury *active_conjury_;

    std::vector<Conjury::Pointer> conjuries_;
};

} // namespace conjure

#endif // CONJURE_STAGE_H_
