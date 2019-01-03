#include <stdio.h>
#include <queue>
#include "conjurer.h"

using namespace conjure;

std::queue<int> q;
bool finished = false;

void Consumer() {
    for (;;) {
        printf("consumer suspend?\n");
        Suspend( []() { return not q.empty() or finished; });
        if (finished) break;
        int a = q.front();
        q.pop();
        printf("Consumer get: %d\n", a);
    }
}

void Producer() {
    for (int i = 0; i < 10; ++i) {
        printf("producer suspend?\n");
        Suspend([]() { return q.empty(); });
        printf("Producer pushing %d\n", i);
        q.push(i);
    }
    puts("producer exit");
    finished = true;
}

int main() {
    q = std::queue<int>{};
    auto producer = Conjure(Config("producer"), Producer);
    Resume(producer);
    Consumer();
}
