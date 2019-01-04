#ifndef CONJURE_EXCEPTIONS_H_
#define CONJURE_EXCEPTIONS_H_

#include "conjure/conjury.h"
#include <sstream>
#include <stdexcept>

namespace conjure {

class Conjury;

class ConjuryException : public std::runtime_error {
  public:
    ConjuryException(Conjury *subject, const std::string &what)
        : std::runtime_error(what), subject_(subject) {}

    const Conjury *Subject() const {
        return subject_;
    }

    const char *what() const noexcept override {
        using namespace std::string_literals;
        if (what_.empty()) {
            what_ = "ConjuryException: "s + subject_->Name();
        }
        return what_.data();
    }

  protected:
    Conjury *subject_;
    mutable std::string what_;
};

class InconsistentWait : public ConjuryException {
  public:
    InconsistentWait(Conjury *waiter, Conjury *waited)
        : ConjuryException(waiter, "inconsistent wait"), waited_(waited) {}

    const char *what() const noexcept override {
        if (what_.empty()) {
            std::ostringstream oss;
            oss << ConjuryException::what() << " wait " << waited_->Name()
                << " who's waited by " << waited_->Parent()->Name();
            what_ = oss.str();
        }
        return what_.data();
    }

  private:
    Conjury *waited_;
};

template <typename G>
class InvalidYieldContext : public ConjuryException {
  public:
    InvalidYieldContext(Conjury *c) : ConjuryException(c, "invalid yield") {}

    const char *what() const noexcept override {
        if (what_.empty()) {
            std::ostringstream oss;
            oss << ConjuryException::what() << " cannot accept a yield of type "
                << typeid(G);
            what_ = oss.str();
        }
        return what_.data();
    }
};

} // namespace conjure

#endif // CONJURE_EXCEPTIONS_H_
