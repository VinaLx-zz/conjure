#ifndef CONJURE_SYSTEM_H_
#define CONJURE_SYSTEM_H_

#include <stdint.h>

namespace conjure::system {

struct CalleeSavedRegs {
    uint64_t rbx = 0;
    uint64_t rbp = 0;
    uint64_t r12 = 0;
    uint64_t r13 = 0;
    uint64_t r14 = 0;
    uint64_t r15 = 0;
};

struct Context {
    Context(void *stack_ptr = nullptr, void *return_addr = nullptr)
        : stack_ptr(stack_ptr), return_addr(return_addr) {}

    CalleeSavedRegs registers;

    void *stack_ptr;
    void *return_addr;
};

} // namespace conjure::system

extern "C" {

void ContextSwitch(
    void *this_, conjure::system::Context *from,
    conjure::system::Context *to) asm("ContextSwitch");

} // extern "C"

#endif // CONJURE_SYSTEM_H_
