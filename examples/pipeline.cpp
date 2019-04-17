#include "conjure/gen-iterator.h"
#include "conjure/interfaces.h"
#include <stdio.h>

using namespace conjure;

Generating<int> OddPrimes(int i) {
    YieldWith(i);
    auto next_odd_primes = Conjure(Config{}, OddPrimes, i + 2);
    for (int n : next_odd_primes) {
        if (n % i != 0) YieldWith(n);
    }
    return {};
}

Generating<int> Primes() {
    YieldWith(2);
    auto odd_primes = Conjure(Config{}, OddPrimes, 3);
    for (int i : odd_primes) {
        YieldWith(i);
    }
    return {};
}

int main() {
    auto primes = Conjure(Config{"primes"}, Primes);
    int c = 0;
    for (int prime : primes) {
        printf("%d ", prime);
        if (++c == 10) break;
    }
    putchar('\n');
}
