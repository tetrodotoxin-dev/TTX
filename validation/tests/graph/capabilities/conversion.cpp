// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/tests/graph/fixtures/conversion.h"

#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/core/view/null_terminated.hpp"

#include "ttx/graph/capabilities/export.hpp"
#include "ttx/graph/capabilities/import.hpp"
#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.h"
#include "validation/tests/graph/fixtures/value.hpp"

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

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    auto& report = *Data::cast<Report>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    return Formats::Uuid(id) == Capabilities::Export::contract_id
               ? ttx_negotiation_status(report.status)
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    const auto status = supports(context, id);
    if (status != TTX_NEGOTIATION_SATISFIED) {
      return status;
    }

    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    Capabilities::Export::Api api = {context, &operations};
    *Data::cast<Capabilities::Export::Api>(output) = api;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &graph_operations};
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
    return Abstract(subject).bind<Validation::Value>().visit(
        [&](Validation::Value value) -> ttx_negotiation_status {
          report.value = value.read();
          return TTX_NEGOTIATION_SATISFIED;
        },

        []() -> ttx_negotiation_status { return TTX_NEGOTIATION_REJECTED; });
  }

  static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, lookup, visit, state};

  static constexpr ttx_export_ops operations = {get_abstract, expose};
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

  static auto read(void* context) -> U8 {
    return Data::cast<ImportedByte>(context)->value;
  }

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Validation::Value::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    auto& subject = *Data::cast<ImportedByte>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    return subject.report && subject.at_root
               ? Report::supports(subject.report, id)
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
      return TTX_NEGOTIATION_SATISFIED;
    }

    auto& subject = *Data::cast<ImportedByte>(context);
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (subject.report && subject.at_root &&
        Formats::Uuid(id) == Capabilities::Export::contract_id) {
      const auto status = Report::supports(subject.report, id);
      if (status == TTX_NEGOTIATION_SATISFIED) {
        *Data::cast<ttx_export>(output) = {context, &export_operations};
      }

      return status;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    auto& subject = *Data::cast<ImportedByte>(context);
    return subject.report && !subject.at_root &&
                   View::Bytes(route) == "report"_view
               ? Report::get_abstract(subject.report)
               : ttx_none();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

  static constexpr ttx_abstract_ops operations = {
    supports, bind, lookup, visit, state};

  static auto expose(void* context, ttx_abstract abstract)
      -> ttx_negotiation_status {
    auto& provider = *Data::cast<ImportedByte>(context);
    return Report::expose(provider.report, abstract);
  }

  static constexpr ttx_export_ops export_operations = {get_abstract, expose};
};

class ByteImport {
 public:
  Report* report = nullptr;
  bool at_root = false;
  Negotiation::Status status = Negotiation::Status::Satisfied;
  Count reads = 0;
  bool observing = false;

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Import::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (Formats::Uuid(id) == Capabilities::Import::contract_id) {
      Capabilities::Import::Api api = {context, &operations};
      *Data::cast<Capabilities::Import::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &graph_operations};
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
    callback(callback_context, ImportedByte::get_abstract(&result));
    subject.observing = false;
    return TTX_NEGOTIATION_SATISFIED;
  }

  static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, lookup, visit_routes, state};

  static constexpr ttx_import_ops operations = {get_abstract, visit};
};

class ByteExport {
 public:
  conversion_output output = {0, 0, 0, TTX_NEGOTIATION_SATISFIED};

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Export::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (Formats::Uuid(id) == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (Formats::Uuid(id) == Capabilities::Export::contract_id) {
      Capabilities::Export::Api api = {context, &operations};
      *Data::cast<Capabilities::Export::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &graph_operations};
  }

  static auto expose(void* context, ttx_abstract subject)
      -> ttx_negotiation_status {
    return conversion_expose(&Data::cast<ByteExport>(context)->output, subject);
  }

  static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, ByteImport::lookup, ByteImport::visit_routes, state};

  static constexpr ttx_export_ops operations = {get_abstract, expose};
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
    copied = graph.bind<Validation::Value>().visit(
        [](Validation::Value value) { return value.read(); },
        [&] {
          EXPECT(False);
          return U8(0);
        });

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
