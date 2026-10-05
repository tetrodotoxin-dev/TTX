// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/concept/fixtures/conversion.h"

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/export.hpp"
#include "ttx/concept/capabilities/import.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.h"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Conversion = {.name = "TTX::Conversion"};

class Report {
 public:
  Count calls = 0;
  U8 value = 0;
  Binding::Status status = Binding::Status::Satisfied;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_binding_status {
    auto& report = *static_cast<Report*>(context);
    if (System::Uuid(id) == Abstract::contract_id) {
      return TTX_BINDING_SATISFIED;
    }

    return System::Uuid(id) == Capabilities::Export::contract_id
               ? static_cast<ttx_binding_status>(report.status)
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    const auto status = supports(context, id);
    if (status != TTX_BINDING_SATISFIED) {
      return status;
    }

    if (System::Uuid(id) == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Data::Form::Storage(output)));
    }

    return static_cast<ttx_binding_status>(
        Binding::provide<Capabilities::Export>(
            {context, &operations}, Data::Form::Storage(output)));
  }

  static auto data(void*) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_none();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static auto expose(void* context, ttx_abstract subject)
      -> ttx_binding_status {
    auto& report = *static_cast<Report*>(context);
    ++report.calls;
    report.value = Abstract(subject).get_data()[0];
    return TTX_BINDING_SATISFIED;
  }

  static constexpr ttx_export_ops operations = {
    {supports, bind, data, resolve, lookup, visit},
    expose};
};

// Each imported answer lives in the visit call. Its reporting service has an
// independent context, so root binding can return that service directly.
class ImportedByte {
 public:
  U8 value;
  Report* report;
  bool at_root;

  static auto data(void* context) -> perimortem_view_bytes {
    return {&static_cast<ImportedByte*>(context)->value, 1};
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_binding_status {
    auto& subject = *static_cast<ImportedByte*>(context);
    if (System::Uuid(id) == Abstract::contract_id) {
      return TTX_BINDING_SATISFIED;
    }

    return subject.report && subject.at_root
               ? Report::supports(subject.report, id)
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    auto& subject = *static_cast<ImportedByte*>(context);
    if (System::Uuid(id) == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Data::Form::Storage(output)));
    }

    return subject.report && subject.at_root
               ? Report::bind(subject.report, id, output)
               : TTX_BINDING_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    auto& subject = *static_cast<ImportedByte*>(context);
    return subject.report && !subject.at_root &&
                   Core::View::Bytes(route.data, route.size) == "report"_view
               ? Report::resolve(subject.report)
               : ttx_none();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

class ByteImport {
 public:
  Report* report = nullptr;
  bool at_root = false;
  Binding::Status status = Binding::Status::Satisfied;
  Count reads = 0;
  bool observing = false;

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Import::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    if (System::Uuid(id) == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Data::Form::Storage(output)));
    }

    if (System::Uuid(id) == Capabilities::Import::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Import>(
              {context, &operations}, Data::Form::Storage(output)));
    }

    return TTX_BINDING_UNKNOWN;
  }

  static auto data(void*) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit_concepts(void*, ttx_concept_visitor) {}

  static auto visit(
      void* context,
      const void* input,
      const ttx_representation* representation,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_binding_status {
    auto& subject = *static_cast<ByteImport*>(context);
    if (subject.status != Binding::Status::Satisfied) {
      return static_cast<ttx_binding_status>(subject.status);
    }

    if (!representation->compatible(
            Data::Form::Compiled<
                Data::Form::Native<U8>::reference>::get_representation())) {
      return TTX_BINDING_REJECTED;
    }

    ++subject.reads;

    ImportedByte result(
        *static_cast<const U8*>(input), subject.report, subject.at_root);
    subject.observing = true;
    callback(callback_context, ImportedByte::resolve(&result));
    subject.observing = false;
    return TTX_BINDING_SATISFIED;
  }

  static constexpr ttx_import_ops operations = {
    {supports, bind, data, resolve, lookup, visit_concepts},
    visit};
};

class ByteExport {
 public:
  conversion_output output = {0, 0, 0, TTX_BINDING_SATISFIED};

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Export::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_binding_status {
    if (System::Uuid(id) == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(Binding::provide<Abstract>(
          resolve(context), Data::Form::Storage(output)));
    }

    if (System::Uuid(id) == Capabilities::Export::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Export>(
              {context, &operations}, Data::Form::Storage(output)));
    }

    return TTX_BINDING_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto expose(void* context, ttx_abstract subject)
      -> ttx_binding_status {
    return conversion_expose(
        &static_cast<ByteExport*>(context)->output, subject);
  }

  static constexpr ttx_export_ops operations = {
    {supports, bind, ByteImport::data, resolve, ByteImport::lookup,
     ByteImport::visit_concepts},
    expose};
};

VALIDATION_TEST(Conversion, scoped_input) {
  ByteImport provider;
  const auto importer =
      Capabilities::Import({&provider, &ByteImport::operations});
  const U8 input = 37;
  const U64 incompatible = 37;
  Count calls = 0;
  U8 copied = 0;
  auto callback = [&](Abstract graph) {
    EXPECT(provider.observing);
    EXPECT(graph.supports<Capabilities::Borrow>() == Binding::Status::Unknown);
    copied = graph.get_data()[0];
    ++calls;
  };
  EXPECT(
      importer.visit(
          &incompatible,
          Data::Form::Compiled<
              Data::Form::Native<U64>::reference>::get_representation(),
          callback) == Binding::Status::Rejected);
  EXPECT_EQ(provider.reads, Count(0));
  EXPECT_EQ(calls, Count(0));
  EXPECT(
      importer.visit(
          &input,
          Data::Form::Compiled<
              Data::Form::Native<U8>::reference>::get_representation(),
          callback) == Binding::Status::Satisfied);
  EXPECT_NOT(provider.observing);
  EXPECT_EQ(copied, input);
  EXPECT_EQ(calls, Count(1));
  const Binding::Status statuses[] = {
    Binding::Status::Unknown, Binding::Status::Rejected};
  for (const auto status : statuses) {
    provider.status = status;
    EXPECT(
        importer.visit(
            &input,
            Data::Form::Compiled<
                Data::Form::Native<U8>::reference>::get_representation(),
            callback) == status);
  }

  EXPECT_EQ(calls, Count(1));
  EXPECT_EQ(provider.reads, Count(1));
}

VALIDATION_TEST(Conversion, foreign_pipeline) {
  Report report;
  ByteImport provider;
  ByteExport terminal;
  const auto importer =
      Capabilities::Import({&provider, &ByteImport::operations});
  const auto exporter =
      Capabilities::Export({&terminal, &ByteExport::operations});
  const U8 input = 81;
  auto run = [&] {
    return conversion_run(
        importer.get_abi(), &input,
        &Data::Form::Compiled<
            Data::Form::Native<U8>::reference>::get_representation(),
        exporter.get_abi());
  };
  EXPECT_EQ(run(), TTX_BINDING_SATISFIED);
  EXPECT_EQ(terminal.output.artifacts, Count(1));
  EXPECT_EQ(terminal.output.value, input);
  EXPECT_EQ(report.calls, Count(0));
  provider.report = &report;
  provider.at_root = true;
  EXPECT_EQ(run(), TTX_BINDING_SATISFIED);
  EXPECT_EQ(report.calls, Count(1));
  provider.at_root = false;
  terminal.output.report_route = 1;
  EXPECT_EQ(run(), TTX_BINDING_SATISFIED);
  EXPECT_EQ(report.calls, Count(2));
  EXPECT_EQ(report.value, input);
  EXPECT_EQ(terminal.output.artifacts, Count(3));
  const Binding::Status refusals[] = {
    Binding::Status::Unknown, Binding::Status::Rejected};
  for (const auto refusal : refusals) {
    report.status = refusal;
    provider.at_root = true;
    terminal.output.report_route = 0;
    EXPECT_EQ(run(), TTX_BINDING_SATISFIED);
    provider.at_root = false;
    terminal.output.report_route = 1;
    EXPECT_EQ(run(), TTX_BINDING_SATISFIED);
  }

  EXPECT_EQ(terminal.output.artifacts, Count(7));
  EXPECT_EQ(report.calls, Count(2));
  const ttx_binding_status statuses[] = {
    TTX_BINDING_UNKNOWN, TTX_BINDING_REJECTED};
  for (const auto status : statuses) {
    terminal.output.status = status;
    EXPECT_EQ(run(), status);
  }

  EXPECT_EQ(terminal.output.artifacts, Count(7));
  EXPECT_EQ(report.calls, Count(2));
}
