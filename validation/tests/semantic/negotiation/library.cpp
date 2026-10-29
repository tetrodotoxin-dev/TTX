// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/test.hpp"

#ifndef TOOLCHAIN_WASM

#include "perimortem/core/data.hpp"
#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/abstract.hpp"
#include "ttx/semantic/negotiation/library.h"
#include "validation/providers/abstract/provider.h"
#include "validation/support/library.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

TTX_DATA_RECORD(
    bootstrap_exports,
    TTX_DATA_MEMBER(bootstrap_exports, context),
    TTX_DATA_MEMBER(bootstrap_exports, visit));
TTX_DATA_RECORD(
    bootstrap_indexed,
    TTX_DATA_MEMBER(bootstrap_indexed, context),
    TTX_DATA_MEMBER(bootstrap_indexed, count),
    TTX_DATA_MEMBER(bootstrap_indexed, get));

static Harness NativeEntry = {.name = "TTX::Bootstrap"};

// This is the public bootstrap sequence. The export is described data. The
// first provider operation is called only after Transfer establishes the
// exported Query's representation and copies its record.
VALIDATION_TEST(NativeEntry, storage_to_query_to_abstract) {
  Allocator::Arena errors;
  const auto path =
      Validation::Support::library_path("libabstract_subject.so"_view);
  auto fail = [](View::Bytes message) { Diagnostics::Log::fatal(message); };

  Process::Library::open(path.get_view(), errors)
      .visit(
          [&](Process::Library& library) {
            library.symbol("ttx_entry"_view, errors)
                .visit(
                    [&](void* symbol) {
                      const auto& published =
                          *Data::cast<const ttx_storage>(symbol);
                      ttx_semantic_query entry{};
                      ttx_storage bootstrap{
                        ttx_semantic_query_representation(),
                        Data::cast<U8>(&entry), sizeof(entry)};

                      ASSERT_EQ(
                          ttx_transfer(&published, &bootstrap),
                          TTX_DATA_SUCCESS);
                      ASSERT(entry.bind && entry.supports);

                      ttx_abstract api{};
                      ttx_storage requested{
                        ttx_abstract_representation(), Data::cast<U8>(&api),
                        sizeof(api)};

                      const perimortem_uuid id{
                        TTX_ABSTRACT_ID_HIGH, TTX_ABSTRACT_ID_LOW};

                      ASSERT_EQ(
                          entry.bind(entry.context, id, requested),
                          TTX_NEGOTIATION_SATISFIED);
                      EXPECT(api.context != entry.context);
                      EXPECT_EQ(
                          api.operations->get_state(api.context),
                          TTX_ABSTRACT_PRESENT);

                      ttx_abstract bound{};
                      ASSERT_EQ(
                          api.operations->bind(api.context, id, &bound),
                          TTX_NEGOTIATION_SATISFIED);
                      EXPECT_EQ(
                          bound.operations->get_state(bound.context),
                          TTX_ABSTRACT_PRESENT);
                    },

                    fail);
          },

          fail);
}

// The export inventory's UUID accepts both callback and indexed APIs. Another
// UUID returns a Query with a different context. Each path reaches the same
// Abstract, establishing that semantic negotiation supports several entry APIs
// and chooses their representation independently of their contract UUID.
VALIDATION_TEST(NativeEntry, independent_paths_and_formats) {
  auto library =
      Validation::Support::open_library("libabstract_subject.so"_view);
  const auto* published = Data::cast<const ttx_storage>(
      Validation::Support::find_symbol(library, "ttx_entry"_view));
  ttx_semantic_query entry{};
  const auto target = Form::Storage::from(entry).get_abi();
  ASSERT_EQ(ttx_transfer(published, &target), TTX_DATA_SUCCESS);

  const Negotiation::Query query(entry);
  const Identity::Uuid previous_abstract(
      0x7cc2e50a46ab4ed7ULL, 0xa2ca04e84f415161ULL);
  EXPECT(query.supports(previous_abstract) == Negotiation::Status::Unknown);

  const Identity::Uuid exports(BOOTSTRAP_EXPORTS_HIGH, BOOTSTRAP_EXPORTS_LOW);
  EXPECT(query.supports(exports) == Negotiation::Status::Satisfied);

  bootstrap_exports callback_api{};
  ASSERT(
      query.bind(exports, Form::Storage::from(callback_api)) ==
      Negotiation::Status::Satisfied);

  Count visits = 0;
  callback_api.visit(
      callback_api.context, &visits, [](void* context, ttx_abstract api) {
        auto& count = *Data::cast<Count>(context);
        if (Abstract(api).get_state() == Abstract::State::Present) {
          ++count;
        }
      });

  EXPECT_EQ(visits, Count(1));

  bootstrap_indexed indexed{};
  ASSERT(
      query.bind(exports, Form::Storage::from(indexed)) ==
      Negotiation::Status::Satisfied);
  ASSERT_EQ(indexed.count(indexed.context), U64(1));
  EXPECT(
      Abstract(indexed.get(indexed.context, 0)).get_state() ==
      Abstract::State::Present);

  ttx_semantic_query next{};
  ASSERT(
      query.bind(
          Identity::Uuid(BOOTSTRAP_QUERY_HIGH, BOOTSTRAP_QUERY_LOW),
          Form::Storage::from(next)) == Negotiation::Status::Satisfied);
  EXPECT(next.context != entry.context);
  Negotiation::Query(next).bind<Abstract>().visit(
      [&](Abstract api) {
        EXPECT(api.get_state() == Abstract::State::Present);
      },

      [&](Negotiation::Status) { EXPECT(False); });

  ttx_abstract wrong{};
  EXPECT(
      query.bind(exports, Form::Storage::from(wrong)) ==
      Negotiation::Status::Rejected);
  EXPECT(
      query.bind(Identity::Uuid(0, 0), Form::Storage::from(wrong)) ==
      Negotiation::Status::Unknown);
  EXPECT(!wrong.context && !wrong.operations);
}

VALIDATION_TEST(NativeEntry, incompatible_bootstrap) {
  auto library =
      Validation::Support::open_library("libabstract_subject.so"_view);
  const auto* published = Data::cast<const ttx_storage>(
      Validation::Support::find_symbol(library, "ttx_entry"_view));
  ttx_abstract wrong{};
  const auto wrong_target = Form::Storage::from(wrong).get_abi();
  EXPECT_EQ(ttx_transfer(published, &wrong_target), TTX_DATA_INCOMPATIBLE);
  EXPECT(!wrong.context && !wrong.operations);

  ttx_semantic_query entry{};
  const auto target = Form::Storage::from(entry).get_abi();
  auto short_source = *published;
  short_source.size = sizeof(entry) - 1;
  EXPECT_EQ(ttx_transfer(&short_source, &target), TTX_DATA_BOUNDS);
  EXPECT(!entry.bind && !entry.supports && !entry.context);
}

// Keep the outer record and every table slot the same size. Only get_state's
// return description differs. Binding rejects that form because the canonical
// description includes the operation table's function signatures.
struct WrongAbstractOperations {
  decltype(ttx_abstract_ops::supports) supports;
  decltype(ttx_abstract_ops::bind) bind;
  decltype(ttx_abstract_ops::resolve_route) resolve_route;
  decltype(ttx_abstract_ops::visit_routes) visit_routes;
  U64 (*get_state)(void*);
};

struct WrongAbstract {
  void* context;
  const WrongAbstractOperations* operations;
};

TTX_DATA_RECORD(
    WrongAbstractOperations,
    TTX_DATA_MEMBER(WrongAbstractOperations, supports),
    TTX_DATA_MEMBER(WrongAbstractOperations, bind),
    TTX_DATA_MEMBER(WrongAbstractOperations, resolve_route),
    TTX_DATA_MEMBER(WrongAbstractOperations, visit_routes),
    TTX_DATA_MEMBER(WrongAbstractOperations, get_state));
TTX_DATA_RECORD(
    WrongAbstract,
    TTX_DATA_MEMBER(WrongAbstract, context),
    TTX_DATA_MEMBER(WrongAbstract, operations));

VALIDATION_TEST(NativeEntry, nested_table_abi) {
  static_assert(sizeof(WrongAbstract) == sizeof(ttx_abstract));
  static_assert(sizeof(WrongAbstractOperations) == sizeof(ttx_abstract_ops));
  const auto& form = Form::Compiled<
      Form::Native<WrongAbstract>::reference>::get_representation();
  WrongAbstract output = WrongAbstract();
  const Form::Storage target(
      ttx_storage(&form.get_abi(), Data::cast<U8>(&output), sizeof(output)));
  auto library =
      Validation::Support::open_library("libabstract_subject.so"_view);
  const auto* published = Data::cast<const ttx_storage>(
      Validation::Support::find_symbol(library, "ttx_entry"_view));
  ttx_semantic_query entry{};
  const auto entry_storage = Form::Storage::from(entry).get_abi();
  ASSERT_EQ(ttx_transfer(published, &entry_storage), TTX_DATA_SUCCESS);

  const Negotiation::Query query(entry);
  EXPECT(
      query.bind(Abstract::contract_id, target) ==
      Negotiation::Status::Rejected);
  EXPECT(output.context == nullptr && output.operations == nullptr);
  query.bind<Abstract>().visit(
      [&](Abstract accepted) {
        EXPECT(accepted.get_state() == Abstract::State::Present);
      },

      [&](Negotiation::Status) { EXPECT(False); });
}

#endif
