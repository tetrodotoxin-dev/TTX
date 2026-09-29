// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/capabilities/create.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/policies/none.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Creation = {.name = "TTX::Create"};

class ScopedCreate {
 public:
  Binding::Status status = Binding::Status::Satisfied;
  mutable bool observing = false;
  auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Capabilities::Create::contract_id ? Binding::Status::Satisfied
                                                   : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Ttx::Data::Form::Storage target) const
      -> Binding::Status {
    return id == Capabilities::Create::contract_id
               ? Binding::provide<Capabilities::Create>(
                     Capabilities::Create::provide(*this).get_abi(), target)
               : Binding::Status::Unknown;
  }
  template <typename Receiver>
  auto create(Abstract, Receiver& receive) const -> Binding::Status {
    if (status != Binding::Status::Satisfied) {
      return status;
    }
    struct Created {
      U8 value = 42;
      auto get_data() const -> Core::View::Bytes {
        return Core::View::Bytes(&value, 1);
      }
    } created;
    observing = true;
    receive(Abstract::provide(created));
    observing = false;
    return Binding::Status::Satisfied;
  }
};

// The created value lives on the provider's stack. Consuming it during the
// callback needs only Create, while keeping the copied byte uses caller
// storage.
VALIDATION_TEST(Creation, scoped_answer) {
  ScopedCreate provider;
  Abstract::provide(provider).bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.supports<Capabilities::Borrow>() ==
            Binding::Status::Unknown);
        U8 copied = 0;
        Count visits = 0;
        auto receive = [&](Abstract subject) {
          EXPECT(provider.observing);
          EXPECT(
              subject.supports<Capabilities::Borrow>() ==
              Binding::Status::Unknown);
          copied = subject.get_data()[0];
          ++visits;
        };
        EXPECT(
            create.create(Policies::None::get_none(), receive) ==
            Binding::Status::Satisfied);
        EXPECT_NOT(provider.observing);
        EXPECT_EQ(copied, U8(42));
        const Binding::Status failures[] = {
          Binding::Status::Unknown, Binding::Status::Rejected};
        for (const auto failure : failures) {
          provider.status = failure;
          EXPECT(create.create(Policies::None::get_none(), receive) == failure);
        }
        EXPECT_EQ(visits, Count(1));
      },
      [&](Binding::Failure) { EXPECT(False); });
}
