#include "coroutine.h"
#include <stdio.h>

using namespace conjure;

void g(int limit) {
    printf("calling g with limit = %d\n", limit);
    for (int i = 0; i < limit; ++i) {
        printf("co g: %d\n", i);
        Yield();
    }
}

void f(int limit) {
    printf("calling f with limit = %d\n", limit);
    auto co = NewRoutine(Config("Gee"), g, 5);
    puts("Gee created");
    for (int i = 0; i < limit; ++i) {
        printf("co f: %d\n", i);
        if (co->GetState() != Conjure::State::kFinished) {
            Resume(co);
        }
        Yield();
    }
}

int main() {
    printf("main pointer: %p\n", &main);
    printf("f pointer: %p\n", &f);
    printf("g pointer: %p\n", &g);
    auto co = NewRoutine(Config("Foo"), f, 10);
    Resume(co);
    puts("preparing...");
    Resume(co);
    for (int i = 0; i < 15; ++i) {
        printf("main: %d\n", i);
        if (co->GetState() != Conjure::State::kFinished) {
            Resume(co);
        }
    }
}
