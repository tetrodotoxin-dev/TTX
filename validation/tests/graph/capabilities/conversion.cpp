// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/graph/fixtures/conversion.h"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/data/transfer.hpp"
#include "ttx/graph/capabilities/export.hpp"
#include "ttx/graph/capabilities/import.hpp"
#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.h"

using namespace Perimortem::Core;
using namespace Perimortem::Serialization;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Conversion = {.name = "TTX::Conversion"};

class Report {
 public:
  Count calls = 0;
  U8 value = 0;
  Negotiation::Status status = Negotiation::Status::Satisfied;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    auto& report = *Data::cast<Report>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    return Formats::Uuid(id) == Capabilities::Export::contract_id
               ? ttx_negotiation_status(report.status)
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
    }

    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    Capabilities::Export::Api api = {context, &operations};
    return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                   Status::Success
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_REJECTED;
  }

  static auto data(void* context) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static auto expose(void* context, ttx_abstract subject)
      -> ttx_negotiation_status {
    auto& report = *Data::cast<Report>(context);
    ++report.calls;
    report.value = Abstract(subject).get_data()[0];
    return TTX_NEGOTIATION_SATISFIED;
  }

  static constexpr ttx_export_ops operations = {
    {supports, bind, data, resolve, lookup, visit},
    expose};
};

// ImportedByte exposes a byte observation during the import callback. At the
// root it delegates Report negotiation to the reporting service, whose context
// differs from the imported value. The resulting API must call the service's
// functions with that service's context.
class ImportedByte {
 public:
  U8 value;
  Report* report;
  bool at_root;

  static auto data(void* context) -> perimortem_view_bytes {
    return {&Data::cast<ImportedByte>(context)->value, 1};
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    auto& subject = *Data::cast<ImportedByte>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    return subject.report && subject.at_root
               ? Report::supports(subject.report, id)
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    auto& subject = *Data::cast<ImportedByte>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return subject.report && subject.at_root
               ? Report::bind(subject.report, id, output)
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    auto& subject = *Data::cast<ImportedByte>(context);
    return subject.report && !subject.at_root &&
                   View::Bytes(route) == "report"_view
               ? Report::resolve(subject.report)
               : ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static constexpr ttx_abstract_ops operations = {supports, bind,   data,
                                                  resolve,  lookup, visit};
};

class ByteImport {
 public:
  Report* report = nullptr;
  bool at_root = false;
  Negotiation::Status status = Negotiation::Status::Satisfied;
  Count reads = 0;
  bool observing = false;

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Import::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (Formats::Uuid(id) == Capabilities::Import::contract_id) {
      Capabilities::Import::Api api = {context, &operations};
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto data(void* context) -> perimortem_view_bytes { return {}; }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit_routes(void* context, ttx_graph_visitor callback) -> void {}

  static auto visit(
      void* context,
      const void* input,
      const ttx_representation* representation,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_negotiation_status {
    auto& subject = *Data::cast<ByteImport>(context);
    if (subject.status != Negotiation::Status::Satisfied) {
      return ttx_negotiation_status(subject.status);
    }

    if (!Form::Representation(*representation)
             .compatible(
                 Form::Compiled<
                     Form::Native<U8>::reference>::get_representation())) {
      return TTX_NEGOTIATION_REJECTED;
    }

    ++subject.reads;

    ImportedByte result(
        *Data::cast<const U8>(input), subject.report, subject.at_root);
    subject.observing = true;
    callback(callback_context, ImportedByte::resolve(&result));
    subject.observing = false;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static constexpr ttx_import_ops operations = {
    {supports, bind, data, resolve, lookup, visit_routes},
    visit};
};

class ByteExport {
 public:
  conversion_output output = {0, 0, 0, TTX_NEGOTIATION_SATISFIED};

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Export::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (Formats::Uuid(id) == Capabilities::Export::contract_id) {
      Capabilities::Export::Api api = {context, &operations};
      return Transfer::copy(Form::Storage::from(api), Form::Storage(output)) ==
                     Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto expose(void* context, ttx_abstract subject)
      -> ttx_negotiation_status {
    return conversion_expose(&Data::cast<ByteExport>(context)->output, subject);
  }

  static constexpr ttx_export_ops operations = {
    {supports, bind, ByteImport::data, resolve, ByteImport::lookup,
     ByteImport::visit_routes},

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
    EXPECT(
        graph.supports<Capabilities::Retain>() == Negotiation::Status::Unknown);
    copied = graph.get_data()[0];
    ++calls;
  };

  EXPECT(
      importer.visit(
          &incompatible,
          Form::Compiled<Form::Native<U64>::reference>::get_representation(),
          callback) == Negotiation::Status::Rejected);
  EXPECT_EQ(provider.reads, Count(0));
  EXPECT_EQ(calls, Count(0));
  EXPECT(
      importer.visit(
          &input,
          Form::Compiled<Form::Native<U8>::reference>::get_representation(),
          callback) == Negotiation::Status::Satisfied);
  EXPECT_NOT(provider.observing);
  EXPECT_EQ(copied, input);
  EXPECT_EQ(calls, Count(1));

  const Negotiation::Status statuses[] = {
    Negotiation::Status::Unknown, Negotiation::Status::Rejected};

  for (const auto status : statuses) {
    provider.status = status;
    EXPECT(
        importer.visit(
            &input,
            Form::Compiled<Form::Native<U8>::reference>::get_representation(),
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
        &Form::Compiled<Form::Native<U8>::reference>::get_representation()
             .get_abi(),
        exporter.get_abi());
  };

  EXPECT_EQ(run(), TTX_NEGOTIATION_SATISFIED);
  EXPECT_EQ(terminal.output.artifacts, Count(1));
  EXPECT_EQ(terminal.output.value, input);
  EXPECT_EQ(report.calls, Count(0));
  provider.report = &report;
  provider.at_root = true;
  EXPECT_EQ(run(), TTX_NEGOTIATION_SATISFIED);
  EXPECT_EQ(report.calls, Count(1));
  provider.at_root = false;
  terminal.output.report_route = 1;
  EXPECT_EQ(run(), TTX_NEGOTIATION_SATISFIED);
  EXPECT_EQ(report.calls, Count(2));
  EXPECT_EQ(report.value, input);
  EXPECT_EQ(terminal.output.artifacts, Count(3));

  const Negotiation::Status refusals[] = {
    Negotiation::Status::Unknown, Negotiation::Status::Rejected};

  for (const auto refusal : refusals) {
    report.status = refusal;
    provider.at_root = true;
    terminal.output.report_route = 0;
    EXPECT_EQ(run(), TTX_NEGOTIATION_SATISFIED);
    provider.at_root = false;
    terminal.output.report_route = 1;
    EXPECT_EQ(run(), TTX_NEGOTIATION_SATISFIED);
  }

  EXPECT_EQ(terminal.output.artifacts, Count(7));
  EXPECT_EQ(report.calls, Count(2));

  const ttx_negotiation_status statuses[] = {
    TTX_NEGOTIATION_UNKNOWN, TTX_NEGOTIATION_REJECTED};

  for (const auto status : statuses) {
    terminal.output.status = status;
    EXPECT_EQ(run(), status);
  }

  EXPECT_EQ(terminal.output.artifacts, Count(7));
  EXPECT_EQ(report.calls, Count(2));
}
