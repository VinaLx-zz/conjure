#ifndef CONJURE_GEN_ITERATOR_H_
#define CONJURE_GEN_ITERATOR_H_

#include "conjure/conjurer.h"
#include <iterator>
#include <optional>

namespace conjure {

struct GenIterEnd {};

template <typename G>
struct GenIterator {

    GenIterator() = default;

    GenIterator(ConjuryClient<ConjureGen<G>> *conjure) : conjure(conjure) {}

    using Self = GenIterator;
    using value_type = G;
    using reference_type = G &;
    using pointer = G *;
    using iterator_category = std::input_iterator_tag;

    G &operator*() {
        return current.value();
    }
    const G &operator*() const {
        return current.value();
    }

    G *operator->() {
        return &*current;
    }
    const G *operator->() const {
        return &*current;
    }

    Self &operator++() {
        current.reset();
        if (GenMoveNext(conjure)) {
            current.emplace(conjure->UnsafeGetGen());
        }
        return *this;
    }

    Self operator++(int _) {
        auto s = *this;
        ++*this;
        return s;
    }

    ConjuryClient<ConjureGen<G>> *conjure;
    std::optional<G> current;
};

template <typename G>
GenIterator<G> begin(ConjuryClient<ConjureGen<G>> *co) {
    GenIterator<G> iter(co);
    ++iter;
    return iter;
}

template <typename G>
GenIterEnd end(ConjuryClient<ConjureGen<G>> *co) {
    return {};
}

template <typename G>
bool operator==(const GenIterator<G> &iter, GenIterEnd e) {
    return not iter.current.has_value();
}

template <typename G>
bool operator==(GenIterEnd e, const GenIterator<G> &iter) {
    return iter == e;
}

template <typename G>
bool operator!=(const GenIterator<G> &iter, GenIterEnd e) {
    return not(iter == e);
}

template <typename G>
bool operator!=(GenIterEnd e, const GenIterator<G> &iter) {
    return not(e == iter);
}

} // namespace conjure

#endif // CONJURE_GEN_ITERATOR_H_
