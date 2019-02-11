#ifndef CONJURE_IO_OPERATION_H_
#define CONJURE_IO_OPERATION_H_

#include "conjure/io/job.h"
#include <fcntl.h>
#include <unistd.h>
#include <string>

namespace conjure::io::job {

struct Open : Job<Open, int> {
    Open(const char *file_path, int flag, int mode)
        : file_path(file_path), flag(flag), mode(mode) {}

    static void Handle(Open &o) {
        o.fd = open(o.file_path, o.flag, o.mode);
    }

    int ReturnValue() {
        return fd;
    }

    int fd = 0;
    const char *file_path;
    int flag;
    int mode;
};

struct Read : Job<Read, int> {
    Read(int fd, void *buffer, int nbyte)
        : fd(fd), buffer(buffer), nbyte(nbyte) {}

    static void Handle(Read &r) {
        r.readn = read(r.fd, r.buffer, r.nbyte);
    }

    int ReturnValue() {
        return readn;
    }

    int fd;
    void *buffer;
    int nbyte;

    int readn = 0;
};

struct Write : Job<Write, int> {
    Write(int fd, const void *buffer, int nbyte)
        : fd(fd), buffer(buffer), nbyte(nbyte) {}

    static void Handle(Write &w) {
        w.writen = write(w.fd, w.buffer, w.nbyte);
    }

    int ReturnValue() {
        return writen;
    }

    int fd;
    const void *buffer;
    int nbyte;

    int writen = 0;
};

} // namespace conjure::io::job

#endif // CONJURE_IO_OPERATION_H_
