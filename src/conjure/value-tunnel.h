#ifndef CONJURE_VALUE_TUNNEL_H_
#define CONJURE_VALUE_TUNNEL_H_

#include <stdio.h>
#include <optional>

namespace conjure {

template <typename T>
class ValueTunnel {
  public:
    bool Empty() const {
        return value_ptr_ == nullptr;
    }

    const T *GetOne() {
        const T *a = value_ptr_;
        value_ptr_ = nullptr;
        return a;
    }

    bool Pass(const T &value) {
        if (Empty()) {
            value_ptr_ = &value;
            return true;
        }
        return false;
    }

  private:
    const T *value_ptr_ = nullptr;
};

} // namespace conjure

#endif // CONJURE_VALUE_TUNNEL_H_
