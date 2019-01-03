#ifndef CONJURE_CONFIG_H_
#define CONJURE_CONFIG_H_

#include <string>

namespace conjure {

struct Config {
    static constexpr int kDefaultStackSize = 16 * 1024;

    Config() = default;

    Config(const std::string &name) : name(name) {}

    int stack_size = kDefaultStackSize;

    std::string name;
};

} // namespace conjure

#endif // CONJURE_CONFIG_H_
