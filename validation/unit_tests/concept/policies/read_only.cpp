// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/read_only.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "validation/unit_tests/concept/policies/read_only_fixture.h"
#include "toolchain/validation/unit_test.hpp"
#include "ttx/abi/operations.hpp"
#include "ttx/abi/receiver.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/create.hpp"
#include "ttx/data/protocol/direct/provider.hpp"
#include "ttx/semantic/transport/flow.hpp"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

TTX_DATA_RECORD(
    test_read_ops,
    TTX_DATA_MEMBER(test_read_ops, abstract),
    TTX_DATA_MEMBER(test_read_ops, value));
TTX_DATA_RECORD(
    test_read,
    TTX_DATA_MEMBER(test_read, source),
    TTX_DATA_MEMBER(test_read, operations));

class Reading : public Abstract {
 public:
  using Api = test_read;
  static constexpr auto contract_id =
      System::Uuid(TEST_READ_ID_HIGH, TEST_READ_ID_LOW);
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->value &&
           Abstract::accept({value.source, &value.operations->abstract});
  }
  explicit Reading(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  auto value() const -> U32 {
    auto api = get_abi();
    return Abi::Operations::from_abstract<test_read_ops>(*api.operations)
        .value(api.source);
  }
};

// This fixture controls route visibility independently of mutation. Composing
// it with ReadOnly exercises both restrictions on the same provider.
class Public : public Abstract {
 public:
  using Api = ttx_abstract;
  using Abstract::Abstract;
  static constexpr auto contract_id =
      System::Uuid(0xf2c9cbbe49b54e41ULL, 0x90a8bf4ce48d48c1ULL);
};

class ProjectedProvider {
 public:
  U32 number = 17;
  auto get_data() const -> Core::View::Bytes { return "unrestricted"_view; }
  auto supports(System::Uuid) const -> Binding::Status {
    return Binding::Status::Satisfied;
  }
  auto bind_interface(System::Uuid, Data::Form::Storage) const
      -> Binding::Status {
    return Binding::Status::Rejected;
  }
  template <typename Policy>
  auto supports(Policy, System::Uuid id) const -> Binding::Status {
    if constexpr (
        Policy::template includes<Public> &&
        !Policy::template includes<Policies::ReadOnly>) {
      if (id == Capabilities::Create::contract_id) {
        return Binding::Status::Satisfied;
      }
    }
    return id == Reading::contract_id ||
                   id == Policies::ReadOnly::contract_id ||
                   id == Public::contract_id ||
                   id == Semantic::Transport::Flow::direct.provider
               ? Binding::Status::Satisfied
               : Binding::Status::Rejected;
  }
  template <typename Policy>
  auto bind_interface(Policy view, System::Uuid id, Data::Form::Storage target)
      -> Binding::Status {
    if constexpr (
        Policy::template includes<Public> &&
        !Policy::template includes<Policies::ReadOnly>) {
      if (id == Capabilities::Create::contract_id) {
        static const ttx_create_ops operations{
          Policy::template operations<ProjectedProvider>(),
          [](void* source, ttx_abstract, void* receiver,
             void (*receive)(void*, ttx_abstract)) -> ttx_binding_status {
            auto& provider = Abi::Receiver::get<ProjectedProvider>(source);
            ++provider.number;
            receive(receiver, Policy::provide(provider).get_abi());
            return TTX_BINDING_SATISFIED;
          }};
        return Binding::provide<Capabilities::Create>(
            {this, &operations}, target);
      }
    }
    if (id == Reading::contract_id) {
      static const test_read_ops operations{
        Policy::template operations<ProjectedProvider>(),
        [](void* source) -> U32 {
          return Abi::Receiver::get<ProjectedProvider>(source).number;
        }};
      return Binding::provide<Reading>({this, &operations}, target);
    }
    if (id == Public::contract_id || id == Policies::ReadOnly::contract_id) {
      if (id == Public::contract_id) {
        return Binding::provide<Abstract>(
            view.template with<Public>(*this).get_abi(), target);
      }
      return Binding::provide<Abstract>(
          view.template with<Policies::ReadOnly>(*this).get_abi(), target);
    }
    if (id == Semantic::Transport::Flow::direct.provider) {
      static const ttx_direct_provider_operations operations{
        [](void*) -> const ttx_representation* {
          return &Data::Form::Compiled<
              Data::Form::Native<U32>::reference>::get_representation();
        },
        [](void* source) -> const void* {
          return &Abi::Receiver::get<ProjectedProvider>(source).number;
        }};
      return Binding::provide<Data::Protocol::Direct::Provider>(
          {this, &operations}, target);
    }
    return Binding::Status::Rejected;
  }
  template <typename Policy>
  auto get_data(Policy) const -> Core::View::Bytes {
    return {reinterpret_cast<const U8*>(&number), sizeof(number)};
  }
  template <typename Policy>
  auto resolve_concept(Policy view, Core::View::Bytes route) const -> Abstract {
    if (route == "public"_view ||
        (!Policy::template includes<Public> && route == "private"_view)) {
      return view;
    }
    return Abstract(ttx_unknown());
  }
  template <typename Policy>
  auto visit_concepts(Policy view, Abstract::Visitor receiver) const -> void {
    receiver("public"_view, view);
    if constexpr (!Policy::template includes<Public>) {
      receiver("private"_view, view);
    }
  }
};

static Toolchain::Validation::Harness tests("TTX::ReadOnly");

// Copying or const qualifying a handle does not restrict the provider's hooks.
// ReadOnly is the separate policy that chooses a smaller visible API.
VALIDATION_TEST(tests, mutable_publication) {
  struct Provider {
    Count observations = 0;
    auto get_data() -> Core::View::Bytes {
      ++observations;
      return "provider"_view;
    }
  } provider;
  const auto handle = Abstract::provide(provider);
  EXPECT_TEXT(handle.get_data(), "provider"_view);
  EXPECT_EQ(provider.observations, Count(1));
  EXPECT(handle.cast<Provider>());
}

VALIDATION_TEST(tests, native_projection) {
  ProjectedProvider provider;
  const auto native = Projection<Policies::ReadOnly>::provide(provider);
  static_assert(sizeof(native) == sizeof(ttx_abstract));
  EXPECT(native.with<Policies::ReadOnly>(provider) == native);
  auto view = Policies::ReadOnly::provide(provider);
  EXPECT(view.get_identity() == &provider);
  EXPECT(!view.cast<ProjectedProvider>());
  EXPECT(view.resolve() == view);
  view.bind<Policies::ReadOnly>().visit(
      [&](auto again) { EXPECT(again == view); },
      [&](Binding::Failure) { EXPECT(false); });
  view.bind<Reading>().visit(
      [&](Reading reading) {
        EXPECT_EQ(reading.value(), 17);
        EXPECT(reading.bind<Abstract>().visit(
            [&](Abstract a) {
              return a.supports<Capabilities::Create>() ==
                     Binding::Status::Rejected;
            },
            [](Binding::Failure) { return false; }));
        reading.bind<Capabilities::Create>().visit(
            [&](auto) { EXPECT(false); },
            [&](Binding::Failure f) {
              EXPECT_EQ(f, Binding::Failure::Rejected);
            });
        EXPECT(
            reading.resolve().supports<Capabilities::Create>() ==
            Binding::Status::Rejected);
      },
      [&](Binding::Failure) { EXPECT(false); });
  provider.number = 23;
  view.bind<Reading>().visit(
      [&](Reading r) { EXPECT_EQ(r.value(), 23); },
      [&](Binding::Failure) { EXPECT(false); });
  EXPECT(view.resolve_concept("private"_view) == view);
}

VALIDATION_TEST(tests, no_implicit_forwarding) {
  struct Provider {
    auto get_data() const -> Core::View::Bytes { return "secret"_view; }
    auto supports(System::Uuid) const -> Binding::Status {
      return Binding::Status::Satisfied;
    }
    auto bind_interface(System::Uuid, Data::Form::Storage) const
        -> Binding::Status {
      return Binding::Status::Satisfied;
    }
  } provider;
  auto view = Policies::ReadOnly::provide(provider);
  EXPECT(view.get_data().is_empty());
  EXPECT_EQ(view.supports<Reading>(), Binding::Status::Unknown);
  view.bind<Reading>().visit(
      [&](Reading) { EXPECT(false); },
      [&](Binding::Failure f) { EXPECT_EQ(f, Binding::Failure::Unknown); });
  Count visits = 0;
  auto receive = [&](Core::View::Bytes, Abstract) { ++visits; };
  view.visit_concepts(Abstract::Visitor(receive));
  EXPECT_EQ(visits, 0);
}

VALIDATION_TEST(tests, composition_and_direct) {
  ProjectedProvider provider;
  auto readonly = Policies::ReadOnly::provide(provider);
  auto public_view = Projection<Public>::provide(provider);
  public_view.bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        auto receive = [&](Abstract value) {
          EXPECT_EQ(
              value.supports<Capabilities::Create>(),
              Binding::Status::Satisfied);
        };
        EXPECT_EQ(
            create.create(public_view, receive), Binding::Status::Satisfied);
      },
      [&](Binding::Failure) { EXPECT(false); });
  EXPECT_EQ(provider.number, 18);
  auto check = [&](Abstract view) {
    EXPECT_EQ(view.supports<Capabilities::Create>(), Binding::Status::Rejected);
    EXPECT(
        view.resolve_concept("private"_view).get_abi().operations ==
        ttx_unknown().operations);
    Count visits = 0;
    auto receive = [&](Core::View::Bytes name, Abstract child) {
      ++visits;
      EXPECT_TEXT(name, "public"_view);
      EXPECT_EQ(
          child.supports<Capabilities::Create>(), Binding::Status::Rejected);
    };
    view.visit_concepts(Abstract::Visitor(receive));
    EXPECT_EQ(visits, 1);
    view.bind<Policies::ReadOnly>().visit(
        [&](auto again) { EXPECT(again == view); },
        [&](Binding::Failure) { EXPECT(false); });
  };
  readonly.bind<Public>().visit(
      [&](Public p) { check(p); }, [&](Binding::Failure) { EXPECT(false); });
  public_view.bind<Policies::ReadOnly>().visit(
      [&](auto p) { check(p); }, [&](Binding::Failure) { EXPECT(false); });
  Semantic::Transport::Flow flow;
  const auto& form = Data::Form::Compiled<
      Data::Form::Native<U32>::reference>::get_representation();
  EXPECT_EQ(
      flow.connect(
          Semantic::Transport::Flow::consumer(form), readonly.get_query()),
      Semantic::Transport::Flow::Status::Success);
  flow.visit(
      [&](const void* p) {
        EXPECT(p == &provider.number);
        EXPECT_EQ(*static_cast<const U32*>(p), 18);
      },
      [&](auto) { EXPECT(false); }, [&](auto) { EXPECT(false); },
      [&](auto) { EXPECT(false); });
}

VALIDATION_TEST(tests, foreign_retention) {
  U32 freed = 0;
  Core::Option<Policies::Borrowed> retained;
  auto observe = [&](Abstract root) {
    root.bind<Policies::ReadOnly>().visit(
        [&](auto projected) { EXPECT(projected == root); },
        [&](Binding::Failure) { EXPECT(false); });
    root.bind<Capabilities::Borrow>().visit(
        [&](auto borrow) {
          borrow.borrow().visit(
              [&](Policies::Borrowed value) { retained = value; },
              [&](Binding::Failure) { EXPECT(false); });
        },
        [&](Binding::Failure) { EXPECT(false); });
  };
  test_read_only_open(
      &Binding::representation<Reading>(), &freed, &observe,
      [](void* state, ttx_abstract value) {
        Ttx::Abi::Receiver::get<decltype(observe)>(state)(Abstract(value));
      });
  EXPECT_EQ(freed, 0);
  EXPECT(retained);
  if (retained) {
    retained->bind<Reading>().visit(
        [&](Reading reading) {
          EXPECT_EQ(reading.value(), 42);
          EXPECT_EQ(
              reading.supports<Policies::Borrowed>(),
              Binding::Status::Satisfied);
          reading.bind<Policies::ReadOnly>().visit(
              [&](Policies::ReadOnly view) {
                EXPECT_EQ(
                    view.supports<Capabilities::Create>(),
                    Binding::Status::Rejected);
                EXPECT_EQ(
                    view.resolve().supports<Policies::Borrowed>(),
                    Binding::Status::Satisfied);
                EXPECT_EQ(
                    view.resolve_concept("x"_view)
                        .supports<Capabilities::Create>(),
                    Binding::Status::Rejected);
              },
              [&](Binding::Failure) { EXPECT(false); });
        },
        [&](Binding::Failure) { EXPECT(false); });
    retained->release();
  }
  EXPECT_EQ(freed, 1);
}
