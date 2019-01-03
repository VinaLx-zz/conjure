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

}

#endif // CONJURE_STATE_H_
