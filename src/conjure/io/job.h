#ifndef CONJURE_IO_JOB_H_
#define CONJURE_IO_JOB_H_

#include "conjure/interfaces.h"
#include <type_traits>

namespace conjure::io {

struct PrimJob {
    using Converter = void (*)(void *, void *);

    void Call() const {
        converter(func, param);
    }

    template <typename Param>
    static PrimJob Make(void (*func)(Param &), Param &p) {
        return PrimJob{reinterpret_cast<void *>(func),
                       reinterpret_cast<void *>(&p),
                       [](void *func, void *param) {
                           reinterpret_cast<void (*)(Param &)>(func)(
                               *reinterpret_cast<Param *>(param));
                       }};
    }

    void *func;
    void *param;
    Converter converter;
};

template <typename JobImpl, typename T>
struct Job {
    Job() : blocking_conjury_(ActiveConjury()) {}

    PrimJob ToPrimitive() {
        return PrimJob::Make(Job::HandleAndSetReady, *this);
    }

    T ReturnValue() {
        return static_cast<JobImpl &>(*this).ReturnValue();
    }

    void BindConjury(Conjury *c) {
        blocking_conjury_ = c;
    }

  private:
    static void HandleAndSetReady(Job &j) {
        JobImpl &ji = static_cast<JobImpl &>(j);
        JobImpl::Handle(ji);
        assert(j->blocking_conjury_ != nullptr);
        j->blocking_conjury_->UnsafeSetState(State::kReady);
    }

    Conjury *blocking_conjury_;
};

} // namespace conjure::io

#endif // CONJURE_IO_JOB_H_
