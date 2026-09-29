// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/create.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
static Toolchain::Validation::Harness Borrowing = {.name = "TTX::Borrowed"};

struct Observations {
  Count borrowed = 0, released = 0, created = 0, binds = 0;
};

// The retained node has its own storage and exposes the same policy through
// every interface. Discovery can end while this node continues to be queried.
class Retained {
 public:
  Observations& counts;
  mutable Count references = 1;
  U8 value;
  auto get_data() const -> Core::View::Bytes {
    return Core::View::Bytes(&value, 1);
  }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Policies::Borrowed::contract_id ||
                   id == Capabilities::Borrow::contract_id ||
                   id == Capabilities::Create::contract_id ||
                   id == Policies::Constant::contract_id
               ? Binding::Status::Satisfied
               : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage requested) const
      -> Binding::Status {
    ++counts.binds;
    if (id == Policies::Borrowed::contract_id) {
      return Binding::provide<Policies::Borrowed>(
          Policies::Borrowed::provide(*this).get_abi(), requested);
    }
    if (id == Capabilities::Borrow::contract_id) {
      return Binding::provide<Capabilities::Borrow>(
          Capabilities::Borrow::provide(*this).get_abi(), requested);
    }
    if (id == Capabilities::Create::contract_id) {
      return Binding::provide<Capabilities::Create>(
          Capabilities::Create::provide(*this).get_abi(), requested);
    }
    if (id == Policies::Constant::contract_id) {
      return Binding::provide<Policies::Constant>(
          Abstract::provide(*this).get_abi(), requested);
    }
    return Binding::Status::Unknown;
  }
  auto borrow() const -> Utility::Result<Policies::Borrowed, Binding::Failure> {
    ++references;
    ++counts.borrowed;
    return Policies::Borrowed::provide(*this);
  }
  template <typename Receiver>
  auto create(Abstract, Receiver& receive) const -> Binding::Status {
    ++counts.created;
    receive(Abstract::provide(*this));
    return Binding::Status::Satisfied;
  }
  auto release() const -> void {
    ++counts.released;
    if (!--references) {
      delete this;
    }
  }
};

class Discovery {
 public:
  Observations& counts;
  U8 value = 7;
  Binding::Status answer = Binding::Status::Satisfied;
  auto get_data() const -> Core::View::Bytes {
    return Core::View::Bytes(&value, 1);
  }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Capabilities::Borrow::contract_id ? Binding::Status::Satisfied
                                                   : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage requested) const
      -> Binding::Status {
    if (id == Capabilities::Borrow::contract_id) {
      return Binding::provide<Capabilities::Borrow>(
          Capabilities::Borrow::provide(*this).get_abi(), requested);
    }
    return Binding::Status::Unknown;
  }
  auto borrow() const -> Utility::Result<Policies::Borrowed, Binding::Failure> {
    if (answer != Binding::Status::Satisfied) {
      return static_cast<Binding::Failure>(answer);
    }
    ++counts.borrowed;
    return Policies::Borrowed::provide(*new Retained(counts, 1, value));
  }
};

VALIDATION_TEST(Borrowing, independent_receiver) {
  Observations counts;
  Core::Option<Policies::Borrowed> retained;
  {
    Discovery source = Discovery(counts);
    const auto weak = Abstract::provide(source);
    weak.bind<Capabilities::Borrow>().visit(
        [&](Capabilities::Borrow request) {
          EXPECT_EQ(counts.borrowed, Count(0));
          EXPECT(request.supports<Abstract>() == Binding::Status::Satisfied);
          request.borrow().visit(
              [&](Policies::Borrowed answer) { retained = answer; },
              [&](Binding::Failure) { EXPECT(False); });
        },
        [&](Binding::Failure) { EXPECT(False); });
    source.value = 12;
    EXPECT_EQ(retained->get_data()[0], U8(7));
  }
  EXPECT_EQ(counts.released, Count(0));
  retained->bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.supports<Policies::Borrowed>() ==
            Binding::Status::Satisfied);
        const auto binds = counts.binds;
        for (Count i = 0; i != 100; ++i) {
          auto receive = [&](Abstract answer) {
            EXPECT_EQ(answer.get_data()[0], U8(7));
          };
          EXPECT(
              create.create(Policies::None::get_none(), receive) ==
              Binding::Status::Satisfied);
        }
        EXPECT_EQ(counts.binds, binds);
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(counts.created, Count(100));
  EXPECT_EQ(counts.borrowed, Count(1));
  EXPECT_EQ(counts.released, Count(0));
  retained->bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed second) {
              EXPECT_EQ(second.get_identity(), retained->get_identity());
              retained->release();
              EXPECT(
                  second.supports<Policies::Constant>() ==
                  Binding::Status::Satisfied);
              EXPECT_EQ(second.get_data()[0], U8(7));
              second.release();
            },
            [&](Binding::Failure) { EXPECT(False); });
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(counts.borrowed, counts.released);
}

VALIDATION_TEST(Borrowing, scoped_and_refused) {
  Observations counts;
  Discovery source = Discovery(counts);
  const auto weak = Abstract::provide(source);
  EXPECT_EQ(weak.get_data()[0], U8(7));
  const Binding::Status answers[] = {
    Binding::Status::Unknown, Binding::Status::Rejected};
  weak.bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        for (const auto answer : answers) {
          source.answer = answer;
          request.borrow().visit(
              [&](Policies::Borrowed) { EXPECT(False); },
              [&](Binding::Failure failure) {
                EXPECT(static_cast<Binding::Status>(failure) == answer);
              });
        }
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(counts.borrowed, Count(0));
  EXPECT_EQ(counts.released, Count(0));
}

// A provider supplied a return operation but an incomplete Abstract table.
// Admission returns the acquired state through that operation before rejecting.
VALIDATION_TEST(Borrowing, malformed_answer_return) {
  struct Owner {
    mutable Count releases = 0;
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto bind_interface(System::Uuid id, Data::Form::Storage target) const
        -> Binding::Status {
      if (id != Capabilities::Borrow::contract_id) {
        return Binding::Status::Unknown;
      }
      static const ttx_borrow_ops operations = ttx_borrow_ops(
          *Abstract::provide(*this).get_abi().operations,
          [](const void* source, ttx_borrowed* output) -> ttx_binding_status {
            static const ttx_borrowed_ops malformed =
                ttx_borrowed_ops({}, [](const void* source) {
                  ++static_cast<const Owner*>(source)->releases;
                });
            *output = ttx_borrowed(source, &malformed);
            return TTX_BINDING_SATISFIED;
          });
      return Binding::provide<Capabilities::Borrow>(
          {this, &operations}, target);
    }
  } owner;
  Abstract::provide(owner).bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed) { EXPECT(False); },
            [&](Binding::Failure failure) {
              EXPECT(failure == Binding::Failure::Rejected);
            });
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(owner.releases, Count(1));
}
