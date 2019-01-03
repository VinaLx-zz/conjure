#ifndef CONJURE_STATE_H_
#define CONJURE_STATE_H_

namespace conjure {

enum class State {
    kInitial,
    kReady,
    kRunning,
    kSuspended,
    kWaiting,
    kFinished
};

namespace state {

constexpr bool IsExecutable(State s) {
    return s == State::kReady or s == State::kInitial;
}

inline const char *ToString(State s) {
    switch (s) {
    case State::kInitial: return "Initial";
    case State::kReady: return "Ready";
    case State::kRunning: return "Running";
    case State::kSuspended: return "Suspended";
    case State::kWaiting: return "Waiting";
    case State::kFinished: return "Finished";
    }
    return "Unknown";
}

} // namespace state

} // namespace conjure

#endif // CONJURE_STATE_H_
