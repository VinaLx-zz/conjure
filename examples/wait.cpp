#include "conjure/conjurer.h"

using namespace conjure;

int MultTwo(int a) {
    printf("MultTwo(%d)\n", a);
    return a * 2;
}

int main() {
    auto co = Conjure(Config(), MultTwo, 2);

    // Control transfer to MultTwo
    int result = Wait(co);

    printf("result: %d\n", result); // 4
}
