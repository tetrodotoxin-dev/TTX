// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Sentinels = {
  .name = "TTX::Sentinels",
};

// Support queries report policy status without binding an API. Unknown
// establishes its own contract while leaving Constant undetermined.
VALIDATION_TEST(Sentinels, support_properties) {
  const auto unknown = Policies::Unknown::get_unknown().get_abstract();
  const auto none = Policies::None::get_none().get_abstract();

  EXPECT(unknown.supports<Abstract>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Unknown>() == Negotiation::Status::Satisfied);
  EXPECT(
      unknown.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT(none.supports<Policies::Constant>() == Negotiation::Status::Unknown);
  EXPECT(none.supports<Policies::Unknown>() == Negotiation::Status::Unknown);
}

// Keep the outer record and every table slot the same size. Only get_data's
// return description differs. Binding rejects that form because the canonical
// description includes the operation table's function signatures.
struct WrongAbstractOperations {
  decltype(ttx_abstract_ops::supports) supports;
  decltype(ttx_abstract_ops::bind) bind;
  U64 (*get_data)(void*);
  decltype(ttx_abstract_ops::resolve) resolve;
  decltype(ttx_abstract_ops::resolve_concept) resolve_concept;
  decltype(ttx_abstract_ops::visit_concepts) visit_concepts;
};

struct WrongAbstract {
  void* context;
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
    TTX_DATA_MEMBER(WrongAbstract, context),
    TTX_DATA_MEMBER(WrongAbstract, operations));

VALIDATION_TEST(Sentinels, nested_table_abi) {
  static_assert(sizeof(WrongAbstract) == sizeof(ttx_abstract));
  static_assert(sizeof(WrongAbstractOperations) == sizeof(ttx_abstract_ops));
  const auto& form = Form::Compiled<
      Form::Native<WrongAbstract>::reference>::get_representation();
  WrongAbstract output = WrongAbstract();
  const Form::Storage target(
      ttx_storage(&form, Data::cast<U8>(&output), sizeof(output)));
  const auto query = Policies::None::get_none().get_abstract().get_query();
  EXPECT(
      query.bind(Abstract::contract_id, target) ==
      Negotiation::Status::Rejected);
  EXPECT(output.context == nullptr && output.operations == nullptr);
  query.bind<Abstract>().visit(
      [&](Abstract accepted) {
        EXPECT(accepted == Policies::None::get_none().get_abstract());
      },

      [&](Negotiation::Status) { EXPECT(False); });
}

// Binding None or Unknown returns their governing Abstract operations. A
// different requested representation is rejected before the output can be used.
VALIDATION_TEST(Sentinels, concept_carrier) {
  const auto none = ttx_none();
  ttx_abstract answer = ttx_abstract();
  const auto& form = Form::Compiled<
      Form::Native<Policies::None::Api>::reference>::get_representation();
  const ttx_storage output =
      ttx_storage(&form, Data::cast<U8>(&answer), sizeof(answer));
  EXPECT_EQ(
      none.operations->bind(none.context, Policies::None::contract_id, output),
      TTX_NEGOTIATION_SATISFIED);

  U32 untouched = 42;
  const auto& operational =
      Form::Compiled<Form::Native<U32>::reference>::get_representation();
  const ttx_storage wrong =
      ttx_storage(&operational, Data::cast<U8>(&untouched), sizeof(untouched));
  EXPECT_EQ(
      none.operations->bind(none.context, Policies::None::contract_id, wrong),
      TTX_NEGOTIATION_REJECTED);
  EXPECT_EQ(untouched, U32(42));

  const auto unknown = ttx_unknown();
  EXPECT_EQ(
      unknown.operations->bind(
          unknown.context, Policies::Unknown::contract_id, output),
      TTX_NEGOTIATION_SATISFIED);
  Abstract(none).bind<Policies::None>().visit(
      [&](Policies::None) {}, [&](Negotiation::Status) { EXPECT(False); });
  Abstract(none).bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });
}

// The Unknown sentinel binds its own Abstract with the same context and byte
// observation. Further negotiation of Constant stays Unknown, preserving the
// sentinel's policy through that bound API.
VALIDATION_TEST(Sentinels, navigation_and_unknown) {
  const auto unknown = Abstract(ttx_unknown());
  unknown.get_query().bind<Abstract>().visit(
      [&](Abstract root) {
        EXPECT(root.get_abi().context == unknown.get_abi().context);
        EXPECT(root.get_data() == "Unknown"_view);
      },

      [&](Negotiation::Status) { EXPECT(False); });

  unknown.bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });

  Policies::None::get_none().get_abstract().bind<Policies::Unknown>().visit(
      [&](Policies::Unknown) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Unknown);
      });
}

// Each tested Concept interface checks its required Abstract operations. A
// provider reporting Satisfied with an empty record is rejected by those
// checks.
VALIDATION_TEST(Sentinels, empty_table_admission) {
  Negotiation::Query query(ttx_semantic_query(
      nullptr,
      [](void*, perimortem_uuid, ttx_storage output) -> ttx_negotiation_status {
        (void)output;
        return TTX_NEGOTIATION_SATISFIED;
      },

      [](void*, perimortem_uuid) -> ttx_negotiation_status {
        return TTX_NEGOTIATION_SATISFIED;
      }));

  query.bind<Policies::Constant>().visit(
      [&](Policies::Constant) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });

  query.bind<Abstract>().visit(
      [&](Abstract) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });
}
