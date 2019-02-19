#ifndef CONJURE_LOG_H_
#define CONJURE_LOG_H_

namespace conjure {

#ifdef CONJURE_VERBOSE
#define CONJURE_LOGF(fmt, ...)                                                 \
    printf("[" __FILE__ ":%d] " fmt "\n", __LINE__, __VA_ARGS__)
#else
#define CONJURE_LOGF(fmt, ...)                                                 \
    do {                                                                       \
    } while (0)
#endif

#define CONJURE_LOGL(str) CONJURE_LOGF(str "%s", "")

} // namespace conjure

#endif // CONJURE_LOG_H_
