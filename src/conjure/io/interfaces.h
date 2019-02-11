#ifndef CONJURE_IO_INTERFACES_H_
#define CONJURE_IO_INTERFACES_H_

#include "conjure/io/operation.h"
#include "conjure/io/worker-pool.h"

namespace conjure::io {

namespace detail {

template <typename JobImpl, typename R>
R SubmitAndSuspend(Job<JobImpl, R> &j) {
    WorkerPool::Instance().Submit(j);
    Suspend();
    return j.ReturnValue();
}

} // namespace detail

int Open(const char *p, int mode) {
    job::Open j(p, mode);
    return detail::SubmitAndSuspend(j);
}

int Read(int fd, void* buffer, int nbyte) {
    job::Read j(fd, buffer, nbyte);
    return detail::SubmitAndSuspend(j);
}

int Write(int fd, const void* buffer, int nbyte) {
    job::Write j(fd, buffer, nbyte);
    return detail::SubmitAndSuspend(j);
}

} // namespace conjure::io

#endif // CONJURE_IO_INTERFACES_H_
