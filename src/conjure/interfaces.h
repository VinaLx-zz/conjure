#ifndef CONJURE_INTERFACES_H_
#define CONJURE_INTERFACES_H_

#include "conjure/conjurer.h"

namespace conjure {

template <typename F, typename... Args>
ConjuryClientT<F, Args...> *
Conjure(const Config &config, F f, Args &&... args) {
    return Conjurer::Instance()->Conjure(
        config, std::move(f), std::forward<Args>(args)...);
}

inline void Yield() {
    Conjurer::Instance()->Yield();
}

inline void End() {
    Conjurer::Instance()->End();
}

inline void Suspend() {
    Conjurer::Instance()->Suspend();
}

inline Conjury *ActiveConjury() {
    return Conjurer::Instance()->ActiveConjury();
}

template <bool kTestFirst = true, typename P>
void SuspendUntil(P p) {
    if constexpr (kTestFirst) {
        if (p()) {
            return;
        }
    }
    Conjurer::Instance()->Suspend(std::move(p));
}

inline bool Resume(Conjury *next) {
    return Conjurer::Instance()->Resume(next);
}

template <typename T>
T Wait(ConjuryClient<T> *co) {
    return Conjurer::Instance()->Wait(co);
}

inline void Wait(Conjury* co) {
    Conjurer::Instance()->Wait(co);
}

template <typename G>
bool GenMoveNext(ConjuryClient<Generating<G>> *co) {
    return Conjurer::Instance()->GenMoveNext(co);
}

template <typename G>
const G *WaitGenerate(ConjuryClient<Generating<G>> *co) {
    if (not GenMoveNext(co)) {
        return nullptr;
    }
    return co->GetGenPtr();
}

template <typename U>
void YieldWith(U &&u) {
    return Conjurer::Instance()->YieldWith(std::forward<U>(u));
}

} // namespace conjure

#endif // CONJURE_INTERFACES_H_
