// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/capabilities/borrow.hpp"
#include "ttx/concept/capabilities/create.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.h"

using namespace Perimortem;
using namespace Ttx;
using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness Borrowing = {.name = "TTX::Borrowed"};

struct Observations {
  Count borrowed = 0, released = 0, created = 0, binds = 0;
};

// Discovery can end while this separately allocated answer remains available.
// All of its tables share these functions because they use the same context.
class Retained {
 public:
  Observations& counts;
  Count references = 1;
  U8 value;

  static auto data(void* context) -> perimortem_view_bytes {
    auto& subject = *static_cast<Retained*>(context);
    return {&subject.value, 1};
  }

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::Borrowed::contract_id ||
                   contract == Capabilities::Borrow::contract_id ||
                   contract == Capabilities::Create::contract_id ||
                   contract == Policies::Constant::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage requested)
      -> ttx_binding_status {
    auto& subject = *static_cast<Retained*>(context);
    ++subject.counts.binds;

    const System::Uuid contract(id);
    const Data::Form::Storage target(requested);
    if (contract == Policies::Borrowed::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Policies::Borrowed>(
              {context, &borrowed_operations}, target));
    }

    if (contract == Capabilities::Borrow::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Borrow>(
              {context, &borrow_operations}, target));
    }

    if (contract == Capabilities::Create::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Create>(
              {context, &create_operations}, target));
    }

    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Abstract>(resolve(context), target));
    }

    return TTX_BINDING_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &abstract_operations};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static auto borrow(void* context, ttx_borrowed* output)
      -> ttx_binding_status {
    auto& subject = *static_cast<Retained*>(context);
    ++subject.references;
    ++subject.counts.borrowed;
    *output = {context, &borrowed_operations};
    return TTX_BINDING_SATISFIED;
  }

  static auto create(
      void* context,
      ttx_abstract,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_binding_status {
    auto& subject = *static_cast<Retained*>(context);
    ++subject.counts.created;
    callback(callback_context, resolve(context));
    return TTX_BINDING_SATISFIED;
  }

  static void release(void* context) {
    auto& subject = *static_cast<Retained*>(context);
    ++subject.counts.released;
    if (!--subject.references) {
      delete &subject;
    }
  }

  static constexpr ttx_abstract_ops abstract_operations = {
    supports, bind, data, resolve, lookup, visit};

  static constexpr ttx_borrowed_ops borrowed_operations = {
    abstract_operations, release};

  static constexpr ttx_borrow_ops borrow_operations = {
    abstract_operations, borrow};

  static constexpr ttx_create_ops create_operations = {
    abstract_operations, create};
};

class Discovery {
 public:
  Observations& counts;
  U8 value = 7;
  Binding::Status answer = Binding::Status::Satisfied;

  static auto data(void* context) -> perimortem_view_bytes {
    auto& subject = *static_cast<Discovery*>(context);
    return {&subject.value, 1};
  }

  static auto supports(void*, perimortem_uuid id) -> ttx_binding_status {
    const System::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Borrow::contract_id
               ? TTX_BINDING_SATISFIED
               : TTX_BINDING_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage requested)
      -> ttx_binding_status {
    const System::Uuid contract(id);
    const Data::Form::Storage target(requested);
    if (contract == Capabilities::Borrow::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Capabilities::Borrow>(
              {context, &operations}, target));
    }

    if (contract == Abstract::contract_id) {
      return static_cast<ttx_binding_status>(
          Binding::provide<Abstract>(resolve(context), target));
    }

    return TTX_BINDING_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void*, perimortem_view_bytes) -> ttx_abstract {
    return ttx_unknown();
  }

  static void visit(void*, ttx_concept_visitor) {}

  static auto borrow(void* context, ttx_borrowed* output)
      -> ttx_binding_status {
    auto& subject = *static_cast<Discovery*>(context);
    if (subject.answer != Binding::Status::Satisfied) {
      return static_cast<ttx_binding_status>(subject.answer);
    }

    ++subject.counts.borrowed;
    *output = {
      new Retained(subject.counts, 1, subject.value),
      &Retained::borrowed_operations};
    return TTX_BINDING_SATISFIED;
  }

  static constexpr ttx_borrow_ops operations = {
    {supports, bind, data, resolve, lookup, visit},
    borrow};
};

VALIDATION_TEST(Borrowing, independent_context) {
  Observations counts;
  Core::Option<Policies::Borrowed> retained;
  {
    Discovery source = Discovery(counts);
    const auto weak = Abstract(Discovery::resolve(&source));
    weak.bind<Capabilities::Borrow>().visit(
        [&](Capabilities::Borrow request) {
          EXPECT_EQ(counts.borrowed, Count(0));
          EXPECT(
              request.get_abstract().supports<Abstract>() ==
              Binding::Status::Satisfied);
          request.borrow().visit(
              [&](Policies::Borrowed answer) { retained = answer; },
              [&](Binding::Failure) { EXPECT(False); });
        },
        [&](Binding::Failure) { EXPECT(False); });

    source.value = 12;
    EXPECT_EQ(retained->get_abstract().get_data()[0], U8(7));
  }

  EXPECT_EQ(counts.released, Count(0));

  retained->get_abstract().bind<Capabilities::Create>().visit(
      [&](Capabilities::Create create) {
        EXPECT(
            create.get_abstract().supports<Policies::Borrowed>() ==
            Binding::Status::Satisfied);

        const auto binds = counts.binds;
        for (Count i = 0; i != 100; ++i) {
          auto callback = [&](Abstract answer) {
            EXPECT_EQ(answer.get_data()[0], U8(7));
          };
          EXPECT(
              create.create(
                  Policies::None::get_none().get_abstract(), callback) ==
              Binding::Status::Satisfied);
        }

        EXPECT_EQ(counts.binds, binds);
      },
      [&](Binding::Failure) { EXPECT(False); });
  EXPECT_EQ(counts.created, Count(100));
  EXPECT_EQ(counts.borrowed, Count(1));
  EXPECT_EQ(counts.released, Count(0));

  retained->get_abstract().bind<Capabilities::Borrow>().visit(
      [&](Capabilities::Borrow request) {
        request.borrow().visit(
            [&](Policies::Borrowed second) {
              EXPECT_EQ(
                  second.get_abstract().get_identity(),
                  retained->get_abstract().get_identity());
              retained->release();
              EXPECT(
                  second.get_abstract().supports<Policies::Constant>() ==
                  Binding::Status::Satisfied);
              EXPECT_EQ(second.get_abstract().get_data()[0], U8(7));
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
  const auto weak = Abstract(Discovery::resolve(&source));
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
  struct Provider {
    Count releases = 0;

    static void release(void* context) {
      ++static_cast<Provider*>(context)->releases;
    }

    static auto borrow(void* context, ttx_borrowed* output)
        -> ttx_binding_status {
      static const ttx_borrowed_ops malformed = {{}, release};
      *output = {context, &malformed};
      return TTX_BINDING_SATISFIED;
    }
  } provider;
  // These Abstract operations use no private state, so the fixture can reuse
  // them with its own context. The acquired answer deliberately omits them.
  const ttx_borrow_ops operations = {*ttx_none().operations, Provider::borrow};
  const Capabilities::Borrow request({&provider, &operations});
  ASSERT(Capabilities::Borrow::accept(request.get_abi()));
  request.borrow().visit(
      [&](Policies::Borrowed) { EXPECT(False); },
      [&](Binding::Failure failure) {
        EXPECT(failure == Binding::Failure::Rejected);
      });
  EXPECT_EQ(provider.releases, Count(1));
}
