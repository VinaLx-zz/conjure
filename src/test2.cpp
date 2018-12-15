#include "./conjurer.h"

using namespace conjure;

int MultTwo(int a) {
    printf("MultTwo(%d)\n", a);
    return a * 2;
}

int main() {
    ConjuryClient<int> *co = Conjure(Config(), MultTwo, 2);
    int result = Wait(co);

    printf("result: %d\n", result);
}
