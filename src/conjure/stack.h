#ifndef CONJURE_STACK_H_
#define CONJURE_STACK_H_

#include <stdint.h>
#include <memory>

namespace conjure {

struct Stack {
    static constexpr int kAlign = 16;

    static char *AlignStack(char *stk) {
        uint64_t s = (uint64_t)stk;
        s = s / kAlign * kAlign;
        s = s - kAlign + sizeof(void *);
        return (char *)s;
    }

    Stack() = default;

    Stack(int64_t stack_size)
        : data(new char[stack_size + kAlign]),
          stack_start(AlignStack(data.get() + (stack_size + kAlign))) {}

    std::unique_ptr<char[]> data;
    char *stack_start = nullptr;
};

} // namespace conjure

#endif // CONJURE_STACK_H_
