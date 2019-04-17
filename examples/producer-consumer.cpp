#include "conjure/gen-iterator.h"
#include "conjure/interfaces.h"
#include <stdio.h>
#include <queue>
#include <vector>

using namespace conjure;

std::queue<int> q;
bool finished = false;
int count = 0;

constexpr int kNProducer = 2;
constexpr int kNConsumer = 3;

void Consume(int consumer, int product) {
    printf("consumer %d: %d\n", consumer, product);
}

void Consumer(int id) {
    for (;;) {
        SuspendUntil<false>([]() { return not q.empty() or finished; });
        if (finished) break;
        int a = q.front();
        q.pop();
        Consume(id, a);
    }
}

Generating<int> MakeProducts(int id, int batch) {
    for (int i = 1; i <= 3; ++i) {
        for (int j = 1; j <= batch; ++j) YieldWith(i + (id * 10));

        SuspendUntil([]() { return q.empty(); });
    }
    return {};
}

void Producer(int id) {
    auto products = Conjure(Config{}, MakeProducts, id, 2);
    for (int product : products) {
        printf("producer %d: %d\n", id, product);
        q.push(product);
    }
    if (++count == kNProducer) {
        finished = true;
    }
}

int main() {
    std::vector<Conjury *> producers(kNProducer);
    std::vector<Conjury *> consumers(kNConsumer);

    for (int i = 0; i < producers.size(); ++i)
        producers[i] = Conjure(Config{}, Producer, i);
    for (int i = 0; i < consumers.size(); ++i)
        consumers[i] = Conjure(Config{}, Consumer, i);

    for (auto p : producers) Resume(p);
    for (auto c : consumers) Resume(c);
    for (auto c : consumers) Wait(c);
}
