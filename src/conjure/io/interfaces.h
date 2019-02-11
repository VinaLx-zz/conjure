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

inline int Open(const char *p, int flag, int mode = 0) {
    constexpr int kDefaultMode =
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    if (flag | O_CREAT and mode == 0) {
        mode = kDefaultMode;
    }
    job::Open j(p, flag, mode);
    return detail::SubmitAndSuspend(j);
}

inline int Read(int fd, void *buffer, int nbyte) {
    job::Read j(fd, buffer, nbyte);
    return detail::SubmitAndSuspend(j);
}

template <size_t N>
int Read(int fd, char (&arr)[N]) {
    int n = Read(fd, arr, N - 1);
    if (n >= 0) {
        arr[n] = '\0';
    }
    return n;
}

inline int Write(int fd, const void *buffer, int nbyte) {
    job::Write j(fd, buffer, nbyte);
    return detail::SubmitAndSuspend(j);
}

template <size_t N>
int Write(int fd, const char (&arr)[N]) {
    return Write(fd, arr, N);
}

} // namespace conjure::io

#endif // CONJURE_IO_INTERFACES_H_
