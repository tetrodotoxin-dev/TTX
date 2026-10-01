// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.hpp"

using namespace Perimortem;
using namespace Ttx;
using namespace Toolchain::Validation;

static Harness Sentinels = {
  .name = "TTX::Sentinels",
};

static_assert(__is_base_of(Concept::Abstract, Concept::Policies::None));
static_assert(__is_base_of(Concept::Abstract, Concept::Policies::Unknown));
static_assert(__is_base_of(Concept::Abstract, Concept::Policies::Constant));

// Marker properties can be observed without negotiating even an Empty API.
// Unknown knows what it is, but other property questions remain provisional.
VALIDATION_TEST(Sentinels, support_properties) {
  using Semantic::Negotiation::Binding::Status;
  const auto unknown = Concept::Policies::Unknown::get_unknown();
  const auto none = Concept::Policies::None::get_none();

  EXPECT(unknown.supports<Concept::Abstract>() == Status::Satisfied);
  EXPECT(unknown.supports<Concept::Policies::Unknown>() == Status::Satisfied);
  EXPECT(unknown.supports<Concept::Policies::Constant>() == Status::Unknown);
  EXPECT(none.supports<Concept::Policies::Constant>() == Status::Unknown);
  EXPECT(none.supports<Concept::Policies::Unknown>() == Status::Unknown);
}

// A policy supplies both questions independently. Navigation may reveal another
// subject, but support stays on the encountered layer rather than resolving
// around a refusal or probing the underlying binding.
VALIDATION_TEST(Sentinels, support_policy) {
  using Semantic::Negotiation::Binding::Status;
  struct Policy {
    Concept::Abstract target;
    Status answer;
    Count binds = 0;
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto resolve() const -> Concept::Abstract { return target; }
    auto supports(System::Uuid id) const -> Status {
      return answer == Status::Unknown ? target.supports(id) : answer;
    }
    auto bind_interface(System::Uuid, Data::Form::Storage) -> Status {
      ++binds;
      return Status::Rejected;
    }
  } policy(Concept::Policies::Unknown::get_unknown(), Status::Unknown);
  const auto subject = Concept::Abstract::provide(policy);

  EXPECT(subject.supports<Concept::Policies::None>() == Status::Unknown);
  policy.target = Concept::Policies::None::get_none();
  policy.answer = Status::Rejected;
  EXPECT(subject.supports<Concept::Policies::None>() == Status::Rejected);
  policy.answer = Status::Unknown;
  EXPECT(subject.supports<Concept::Policies::None>() == Status::Satisfied);
  EXPECT_EQ(policy.binds, Count(0));
}

// Keep the outer record and every table slot the same size. Only get_data's
// result differs, so rejection proves that agreement follows the typed table
// pointer rather than treating the Abstract as two opaque pointer slots.
struct WrongAbstractOperations {
  decltype(ttx_abstract_ops::supports) supports;
  decltype(ttx_abstract_ops::bind) bind;
  U64 (*get_data)(void*);
  decltype(ttx_abstract_ops::resolve) resolve;
  decltype(ttx_abstract_ops::resolve_concept) resolve_concept;
  decltype(ttx_abstract_ops::visit_concepts) visit_concepts;
};

struct WrongAbstract {
  void* source;
  const WrongAbstractOperations* operations;
};

TTX_DATA_RECORD(
    WrongAbstractOperations,
    TTX_DATA_MEMBER(WrongAbstractOperations, supports),
    TTX_DATA_MEMBER(WrongAbstractOperations, bind),
    TTX_DATA_MEMBER(WrongAbstractOperations, get_data),
    TTX_DATA_MEMBER(WrongAbstractOperations, resolve),
    TTX_DATA_MEMBER(WrongAbstractOperations, resolve_concept),
    TTX_DATA_MEMBER(WrongAbstractOperations, visit_concepts));
TTX_DATA_RECORD(
    WrongAbstract,
    TTX_DATA_MEMBER(WrongAbstract, source),
    TTX_DATA_MEMBER(WrongAbstract, operations));

VALIDATION_TEST(Sentinels, nested_table_abi) {
  static_assert(sizeof(WrongAbstract) == sizeof(ttx_abstract));
  static_assert(sizeof(WrongAbstractOperations) == sizeof(ttx_abstract_ops));
  const auto& form = Data::Form::Compiled<
      Data::Form::Native<WrongAbstract>::reference>::get_representation();
  WrongAbstract output = WrongAbstract();
  const Data::Form::Storage target(
      ttx_storage(&form, reinterpret_cast<U8*>(&output), sizeof(output)));
  const auto query = Concept::Policies::None::get_none().get_query();
  EXPECT(
      query.bind(Concept::Abstract::contract_id, target) ==
      Semantic::Negotiation::Binding::Status::Rejected);
  EXPECT(output.source == nullptr && output.operations == nullptr);
  query.bind<Concept::Abstract>().visit(
      [&](Concept::Abstract accepted) {
        EXPECT(accepted == Concept::Policies::None::get_none());
      },
      [&](Semantic::Negotiation::Binding::Failure) { EXPECT(False); });
}

// Binding an answer preserves its Abstract operations. A different data form
// still requires rejection before the caller can use the returned table.
VALIDATION_TEST(Sentinels, concept_carrier) {
  const auto none = ttx_none();
  ttx_abstract answer = ttx_abstract();
  const auto& form =
      Semantic::Negotiation::Binding::representation<Concept::Policies::None>();
  const ttx_storage output =
      ttx_storage(&form, reinterpret_cast<U8*>(&answer), sizeof(answer));
  EXPECT_EQ(
      none.operations->bind(
          none.source, Concept::Policies::None::contract_id, output),
      TTX_BINDING_SATISFIED);

  U32 untouched = 42;
  const auto& operational = Data::Form::Compiled<
      Data::Form::Native<U32>::reference>::get_representation();
  const ttx_storage wrong = ttx_storage(
      &operational, reinterpret_cast<U8*>(&untouched), sizeof(untouched));
  EXPECT_EQ(
      none.operations->bind(
          none.source, Concept::Policies::None::contract_id, wrong),
      TTX_BINDING_REJECTED);
  EXPECT_EQ(untouched, U32(42));

  const auto unknown = ttx_unknown();
  EXPECT_EQ(
      unknown.operations->bind(
          unknown.source, Concept::Policies::Unknown::contract_id, output),
      TTX_BINDING_SATISFIED);
  Concept::Abstract(none).bind<Concept::Policies::None>().visit(
      [&](Concept::Policies::None) {},
      [&](Semantic::Negotiation::Binding::Failure) { EXPECT(False); });
  Concept::Abstract(none).bind<Concept::Policies::Constant>().visit(
      [&](Concept::Policies::Constant) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Unknown);
      });
}

// Marker negotiation does not replace navigation. Abstract still binds its
// own table, and Unknown preserves uncertainty for questions it cannot settle.
VALIDATION_TEST(Sentinels, navigation_and_unknown) {
  const auto unknown = Concept::Abstract(ttx_unknown());
  unknown.get_query().bind<Concept::Abstract>().visit(
      [&](Concept::Abstract root) {
        EXPECT(root.get_identity() == unknown.get_identity());
        EXPECT(root.get_data() == "Unknown"_view);
      },
      [&](Semantic::Negotiation::Binding::Failure) { EXPECT(False); });

  unknown.bind<Concept::Policies::Constant>().visit(
      [&](Concept::Policies::Constant) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Unknown);
      });
  Concept::Policies::None::get_none().bind<Concept::Policies::Unknown>().visit(
      [&](Concept::Policies::Unknown) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Unknown);
      });
}

// Every Concept result supplies a usable Abstract table. An accepted status
// with an empty result fails admission for both policies and base Abstract.
VALIDATION_TEST(Sentinels, empty_table_admission) {
  Semantic::Negotiation::Query query(ttx_semantic_query(
      nullptr,
      [](void*, perimortem_uuid,
         ttx_storage output) -> ttx_binding_status {
        (void)output;
        return TTX_BINDING_SATISFIED;
      },
      [](void*, perimortem_uuid) -> ttx_binding_status {
        return TTX_BINDING_SATISFIED;
      }));
  query.bind<Concept::Policies::Constant>().visit(
      [&](Concept::Policies::Constant) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Rejected);
      });
  query.bind<Concept::Abstract>().visit(
      [&](Concept::Abstract) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Rejected);
      });
}

// Abstract adapts a native provider through its operation table. The resulting
// view preserves the child's policy through both navigation paths and
// negotiates the same marker as the C provider.
VALIDATION_TEST(Sentinels, native_publication) {
  struct Leaf {
    auto get_data() const -> Core::View::Bytes { return "value"_view; }
    auto supports(System::Uuid id) const
        -> Semantic::Negotiation::Binding::Status {
      using Semantic::Negotiation::Binding::Status;
      return id == Concept::Policies::Constant::contract_id ? Status::Satisfied
                                                            : Status::Rejected;
    }
    auto bind_interface(System::Uuid id, Data::Form::Storage requested)
        -> Semantic::Negotiation::Binding::Status {
      if (id == Concept::Policies::Constant::contract_id) {
        return Semantic::Negotiation::Binding::provide<
            Concept::Policies::Constant>(
            Concept::Abstract::provide(*this).get_abi(), requested);
      }

      return Semantic::Negotiation::Binding::Status::Rejected;
    }
  } leaf;
  struct Parent {
    Leaf& leaf;
    auto get_data() const -> Core::View::Bytes { return "parent"_view; }
    auto resolve_concept(Core::View::Bytes name) const -> Concept::Abstract {
      return name == leaf.get_data() ? Concept::Abstract::provide(leaf)
                                     : Concept::Policies::None::get_none();
    }
    auto visit_concepts(Concept::Abstract::Visitor visitor) const -> void {
      visitor(leaf.get_data(), Concept::Abstract::provide(leaf));
    }
  } parent(leaf);

  const auto root = Concept::Abstract::provide(parent);
  const auto child = root.resolve_concept("value"_view);
  child.bind<Concept::Policies::Constant>().visit(
      [](Concept::Policies::Constant) {},
      [&](Semantic::Negotiation::Binding::Failure) { EXPECT(False); });
  child.bind<Concept::Policies::Unknown>().visit(
      [&](Concept::Policies::Unknown) { EXPECT(False); },
      [&](Semantic::Negotiation::Binding::Failure failure) {
        EXPECT(failure == Semantic::Negotiation::Binding::Failure::Rejected);
      });

  Count visited = 0;
  auto visitor = [&](Core::View::Bytes name, Concept::Abstract subject) {
    EXPECT(name == "value"_view);
    EXPECT(subject.get_identity() == child.get_identity());
    ++visited;
  };
  root.visit_concepts(Concept::Abstract::Visitor(visitor));
  EXPECT_EQ(visited, Count(1));
  child.resolve_concept("unspecified"_view)
      .bind<Concept::Policies::Unknown>()
      .visit(
          [](Concept::Policies::Unknown) {},
          [&](Semantic::Negotiation::Binding::Failure) { EXPECT(False); });
}

// The same None node serves many rejected routes. Its singleton identity says
// nothing about a provider's next answer to any one of those questions.
VALIDATION_TEST(Sentinels, rejection_can_change) {
  using Concept::Abstract;
  using Semantic::Negotiation::Binding::Status;
  const auto none = Concept::Policies::None::get_none();
  EXPECT(none.resolve() == none);
  EXPECT(none.resolve_concept("anything"_view) == none);
  EXPECT(none.resolve_concept(""_view) == none);
  EXPECT(none.supports<Concept::Policies::Constant>() == Status::Unknown);

  struct Provider {
    Abstract answer;
    auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
    auto resolve_concept(Core::View::Bytes) const -> Abstract { return answer; }
  } provider(none);
  const auto subject = Abstract::provide(provider);
  EXPECT(subject.resolve_concept("value"_view) == none);
  provider.answer = subject;
  EXPECT(subject.resolve_concept("value"_view) == subject);
}
