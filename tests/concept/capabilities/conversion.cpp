// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/concept/fixtures/conversion.h"

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/export.hpp"
#include "ttx/concept/capabilities/import.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Conversion = {.name = "TTX::Conversion"};

class DefaultImport {
 public:
  auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Capabilities::Import::contract_id ? Binding::Status::Satisfied
                                                   : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage output) const
      -> Binding::Status {
    return id == Capabilities::Import::contract_id
               ? Binding::provide<Capabilities::Import>(
                     Capabilities::Import::provide(*this).get_abi(), output)
               : Binding::Status::Unknown;
  }
};

class Report {
 public:
  auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  mutable Count calls = 0;
  mutable U8 value = 0;
  Binding::Status status = Binding::Status::Satisfied;
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Capabilities::Export::contract_id ? status
                                                   : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage output) const
      -> Binding::Status {
    if (id == Capabilities::Export::contract_id &&
        status != Binding::Status::Satisfied) {
      return status;
    }
    return id == Capabilities::Export::contract_id
               ? Binding::provide<Capabilities::Export>(
                     Capabilities::Export::provide(*this).get_abi(), output)
               : Binding::Status::Unknown;
  }
  auto expose(Abstract subject) const -> Binding::Status {
    ++calls;
    value = subject.get_data()[0];
    return Binding::Status::Satisfied;
  }
};

// Each imported answer lives on visit's stack. The reporting service belongs
// to the test caller and can be published on that answer or one of its routes.
class ImportedByte {
 public:
  U8 value;
  Report* report;
  bool at_root;
  auto get_data() const -> Core::View::Bytes {
    return Core::View::Bytes(&value, 1);
  }
  auto supports(System::Uuid id) const -> Binding::Status {
    return report && at_root ? report->supports(id) : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage output) const
      -> Binding::Status {
    if (report && at_root && id == Capabilities::Export::contract_id) {
      const auto status = report->supports(id);
      if (status != Binding::Status::Satisfied) {
        return status;
      }
      return Binding::provide<Capabilities::Export>(
          Capabilities::Export::provide(*this).get_abi(), output);
    }
    return Binding::Status::Unknown;
  }
  auto expose(Abstract subject) const -> Binding::Status {
    return report->expose(subject);
  }
  auto resolve_concept(Core::View::Bytes route) const -> Abstract {
    return report && !at_root && route == "report"_view
               ? Abstract::provide(*report)
               : Policies::None::get_none();
  }
};

class ByteImport {
 public:
  auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  Report* report = nullptr;
  bool at_root = false;
  Binding::Status status = Binding::Status::Satisfied;
  mutable Count reads = 0;
  mutable bool observing = false;
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Capabilities::Import::contract_id ? Binding::Status::Satisfied
                                                   : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Data::Form::Storage output) const
      -> Binding::Status {
    return id == Capabilities::Import::contract_id
               ? Binding::provide<Capabilities::Import>(
                     Capabilities::Import::provide(*this).get_abi(), output)
               : Binding::Status::Unknown;
  }
  template <typename Receive>
  auto visit(
      const void* input,
      const Data::Form::Representation& representation,
      Receive& receive) const -> Binding::Status {
    if (status != Binding::Status::Satisfied) {
      return status;
    }
    if (!representation.compatible(
            Data::Form::Compiled<
                Data::Form::Native<U8>::reference>::get_representation())) {
      return Binding::Status::Rejected;
    }
    ++reads;
    const ImportedByte result =
        ImportedByte(*static_cast<const U8*>(input), report, at_root);
    observing = true;
    receive(Abstract::provide(result));
    observing = false;
    return Binding::Status::Satisfied;
  }
};

class ByteExport {
 public:
  auto get_data() const -> Core::View::Bytes { return Core::View::Bytes(); }
  mutable conversion_output output =
      conversion_output(0, 0, 0, TTX_BINDING_SATISFIED);
  auto expose(Abstract subject) const -> Binding::Status {
    return static_cast<Binding::Status>(
        conversion_expose(&output, subject.get_abi()));
  }
};

VALIDATION_TEST(Conversion, default_import) {
  const DefaultImport provider;
  Abstract::provide(provider).bind<Capabilities::Import>().visit(
      [&](Capabilities::Import importer) {
        EXPECT(importer.supports<Abstract>() == Binding::Status::Satisfied);
        EXPECT(
            importer.supports<Capabilities::Borrow>() ==
            Binding::Status::Unknown);
        Count calls = 0, routes = 0;
        auto receive = [&](Abstract graph) {
          ++calls;
          EXPECT(
              graph.supports<Policies::None>() == Binding::Status::Satisfied);
          EXPECT(
              graph.supports<Policies::Constant>() == Binding::Status::Unknown);
          EXPECT(
              graph.supports<Capabilities::Borrow>() ==
              Binding::Status::Unknown);
          auto route = [&](Core::View::Bytes, Abstract) { ++routes; };
          graph.visit_concepts(Abstract::Visitor(route));
          EXPECT(graph.resolve_concept("anything"_view) == graph);
        };
        const U64 input = 42;
        EXPECT(
            importer.visit(
                &input,
                Data::Form::Compiled<
                    Data::Form::Native<U64>::reference>::get_representation(),
                receive) == Binding::Status::Satisfied);
        EXPECT_EQ(calls, Count(1));
        EXPECT_EQ(routes, Count(0));
      },
      [&](Binding::Failure) { EXPECT(False); });
}

VALIDATION_TEST(Conversion, scoped_input) {
  ByteImport provider;
  const auto importer = Capabilities::Import::provide(provider);
  const U8 input = 37;
  const U64 incompatible = 37;
  Count calls = 0;
  U8 copied = 0;
  auto receive = [&](Abstract graph) {
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
          receive) == Binding::Status::Rejected);
  EXPECT_EQ(provider.reads, Count(0));
  EXPECT_EQ(calls, Count(0));
  EXPECT(
      importer.visit(
          &input,
          Data::Form::Compiled<
              Data::Form::Native<U8>::reference>::get_representation(),
          receive) == Binding::Status::Satisfied);
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
            receive) == status);
  }
  EXPECT_EQ(calls, Count(1));
  EXPECT_EQ(provider.reads, Count(1));
}

VALIDATION_TEST(Conversion, foreign_pipeline) {
  Report report;
  ByteImport provider;
  ByteExport terminal;
  const auto importer = Capabilities::Import::provide(provider);
  const auto exporter = Capabilities::Export::provide(terminal);
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
