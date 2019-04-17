// This example shows that we can pass references between coroutines even if we
// are yielding a temporary object.
// Temporary objects are destructed only when the yielding coroutine is resumed.
// So it's safe to use it in other coroutines. And this eliminate all copies
// when yielding.

#include "conjure/gen-iterator.h"
#include "conjure/interfaces.h"
#include <iostream>

using namespace conjure;

struct Noise {
    Noise(int v) : val(v) {
        printf("Noise %d construct\n", val);
    }
    Noise(const Noise &n) {
        val = n.val;
        printf("Noise %d copy\n", val);
    }
    Noise(Noise &&n) {
        val = n.val;
        printf("Noise %d move\n", val);
    }
    ~Noise() {
        printf("Noise %d destruct\n", val);
    }
    int val;
};

Generating<Noise> TestLifeCycle(int a) {
    for (int i = 0; i < a; ++i) {
        YieldWith(Noise{i});
    }
    return {};
}

int main() {
    auto noisy_gen = Conjure(Config(), TestLifeCycle, 5);
    for (auto &n : noisy_gen) {
        printf("Get value %d\n", n.val);
    }
}
