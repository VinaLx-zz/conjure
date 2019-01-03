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

} // namespace state

} // namespace conjure

#endif // CONJURE_STATE_H_
