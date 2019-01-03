#include "conjurer.h"
#include <queue>
#include <stdio.h>

using namespace conjure;

std::queue<int> q;
bool finished = false;

void Consumer() {
    for (;;) {
        printf("consumer suspend?\n");
        Suspend([]() { return not q.empty() or finished; });
        if (finished) {
            printf("consumer exit\n");
            break;
        }
        int a = q.front();
        q.pop();
        printf("consumer get: %d\n", a);
    }
}

void Producer() {
    for (int i = 0; i < 10; ++i) {
        printf("producer suspend?\n");
        Suspend([]() { return q.empty(); });
        printf("producer pushing %d\n", i);
        q.push(i);
    }
    finished = true;
    puts("producer exit");
}

int main() {
    q = std::queue<int>{};
    auto producer = Conjure(Config("producer"), Producer);
    auto consumer = Conjure(Config("consumer"), Consumer);
    Resume(producer);
    Resume(consumer);
    Wait(consumer);
}
