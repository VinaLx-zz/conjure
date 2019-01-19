#ifndef CONJURE_IO_JOB_H_
#define CONJURE_IO_JOB_H_

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

template <typename JobImpl>
struct Job {
    PrimJob EraseType() {
        return PrimJob::Make(JobImpl::Handle, static_cast<JobImpl *>(this));
    }
};

} // namespace conjure::io

#endif // CONJURE_IO_JOB_H_
