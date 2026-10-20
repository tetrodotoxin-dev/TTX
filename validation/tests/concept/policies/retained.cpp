// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/memory/lifetime.hpp"
#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/memory/allocator/global.hpp"

#include "ttx/concept/capabilities/create.hpp"
#include "ttx/concept/capabilities/retain.hpp"
#include "ttx/concept/policies/constant.hpp"
#include "ttx/concept/policies/none.hpp"
#include "ttx/concept/policies/unknown.h"
#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Serialization;
using namespace Toolchain::Memory;
using namespace Toolchain::Validation;
using namespace Ttx::Concept;
using namespace Ttx::Data;
using namespace Ttx::Semantic;

static Harness Retention = {.name = "TTX::Retained"};

struct Observations {
  Count acquisitions = 0, released = 0, created = 0, binds = 0;
};

// Discovery can end while this separately allocated provider remains available.
// All of its tables share these functions because they use the same context.
class Retained {
 public:
  Observations& counts;
  Count references = 1;
  U8 value;

  static auto data(void* context) -> perimortem_view_bytes {
    auto& subject = *Data::cast<Retained>(context);
    return {&subject.value, 1};
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::Retained::contract_id ||
                   contract == Capabilities::Retain::contract_id ||
                   contract == Capabilities::Create::contract_id ||
                   contract == Policies::Constant::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage requested)
      -> ttx_negotiation_status {
    auto& subject = *Data::cast<Retained>(context);
    ++subject.counts.binds;

    const Formats::Uuid contract(id);
    const Form::Storage target(requested);
    if (contract == Policies::Retained::contract_id) {
      Policies::Retained::Api api = {context, &retained_operations};
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Capabilities::Retain::contract_id) {
      Capabilities::Retain::Api api = {context, &retain_operations};
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Capabilities::Create::contract_id) {
      Capabilities::Create::Api api = {context, &create_operations};
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &abstract_operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_concept_visitor callback) -> void {}

  static auto retain(void* context, ttx_retained* output)
      -> ttx_negotiation_status {
    auto& subject = *Data::cast<Retained>(context);
    ++subject.references;
    ++subject.counts.acquisitions;
    *output = {context, &retained_operations};
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto create(
      void* context,
      ttx_abstract,
      void* callback_context,
      void (*callback)(void*, ttx_abstract)) -> ttx_negotiation_status {
    auto& subject = *Data::cast<Retained>(context);
    ++subject.counts.created;
    callback(callback_context, resolve(context));
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto release(void* context) -> void {
    auto& subject = *Data::cast<Retained>(context);
    ++subject.counts.released;
    if (!--subject.references) {
      U8* storage = Data::cast<U8>(&subject);
      subject.~Retained();
      Allocator::Global::release(storage);
    }
  }

  static constexpr ttx_abstract_ops abstract_operations = {
    supports, bind, data, resolve, lookup, visit};

  static constexpr ttx_retained_ops retained_operations = {
    abstract_operations, release};

  static constexpr ttx_retain_ops retain_operations = {
    abstract_operations, retain};

  static constexpr ttx_create_ops create_operations = {
    abstract_operations, create};
};

static_assert(alignof(Retained) <= Allocator::Global::allocation_alignment);

class Discovery {
 public:
  Observations& counts;
  U8 value = 7;
  Negotiation::Status answer = Negotiation::Status::Satisfied;

  static auto data(void* context) -> perimortem_view_bytes {
    auto& subject = *Data::cast<Discovery>(context);
    return {&subject.value, 1};
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Retain::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, ttx_storage requested)
      -> ttx_negotiation_status {
    const Formats::Uuid contract(id);
    const Form::Storage target(requested);
    if (contract == Capabilities::Retain::contract_id) {
      Capabilities::Retain::Api api = {context, &operations};
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    if (contract == Abstract::contract_id) {
      Abstract::Api api = resolve(context);
      return Transfer::copy(Form::Storage::from(api), target) == Status::Success
                 ? TTX_NEGOTIATION_SATISFIED
                 : TTX_NEGOTIATION_REJECTED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto resolve(void* context) -> ttx_abstract {
    return {context, &operations.abstract};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_concept_visitor callback) -> void {}

  static auto retain(void* context, ttx_retained* output)
      -> ttx_negotiation_status {
    auto& subject = *Data::cast<Discovery>(context);
    if (subject.answer != Negotiation::Status::Satisfied) {
      return ttx_negotiation_status(subject.answer);
    }

    ++subject.counts.acquisitions;

    const auto allocation = Allocator::Global::allocate(sizeof(Retained));
    auto& retained = Lifetime::construct<Retained>(allocation.get_data(), [&] {
      return Retained(subject.counts, 1, subject.value);
    });

    *output = {&retained, &Retained::retained_operations};

    return TTX_NEGOTIATION_SATISFIED;
  }

  static constexpr ttx_retain_ops operations = {
    {supports, bind, data, resolve, lookup, visit},
    retain};
};

VALIDATION_TEST(Retention, independent_context) {
  Observations counts;
  Storage::Option<Policies::Retained> retained;
  {
    Discovery source = Discovery(counts);
    const auto weak = Abstract(Discovery::resolve(&source));
    weak.bind<Capabilities::Retain>().visit(
        [&](Capabilities::Retain request) {
          EXPECT_EQ(counts.acquisitions, Count(0));
          EXPECT(
              request.get_abstract().supports<Abstract>() ==
              Negotiation::Status::Satisfied);
          request.retain().visit(
              [&](Policies::Retained answer) { retained = answer; },
              [&](Negotiation::Status) { EXPECT(False); });
        },

        [&](Negotiation::Status) { EXPECT(False); });

    source.value = 12;
    retained.visit(
        [&](Policies::Retained acquired) {
          EXPECT_EQ(acquired.get_abstract().get_data()[0], U8(7));
        },

        [&] { EXPECT(False); });
  }

  EXPECT_EQ(counts.released, Count(0));

  retained.visit(
      [&](Policies::Retained acquired) {
        acquired.get_abstract().bind<Capabilities::Create>().visit(
            [&](Capabilities::Create create) {
              EXPECT(
                  create.get_abstract().supports<Policies::Retained>() ==
                  Negotiation::Status::Satisfied);

              const auto binds = counts.binds;
              for (Count i = 0; i != 100; ++i) {
                auto callback = [&](Abstract answer) {
                  EXPECT_EQ(answer.get_data()[0], U8(7));
                };

                EXPECT(
                    create.create(
                        Policies::None::get_none().get_abstract(), callback) ==
                    Negotiation::Status::Satisfied);
              }

              EXPECT_EQ(counts.binds, binds);
            },

            [&](Negotiation::Status) { EXPECT(False); });
        EXPECT_EQ(counts.created, Count(100));
        EXPECT_EQ(counts.acquisitions, Count(1));
        EXPECT_EQ(counts.released, Count(0));

        acquired.get_abstract().bind<Capabilities::Retain>().visit(
            [&](Capabilities::Retain request) {
              request.retain().visit(
                  [&](Policies::Retained second) {
                    EXPECT_EQ(
                        second.get_abstract().get_abi().context,
                        acquired.get_abstract().get_abi().context);
                    acquired.release();
                    EXPECT(
                        second.get_abstract().supports<Policies::Constant>() ==
                        Negotiation::Status::Satisfied);
                    EXPECT_EQ(second.get_abstract().get_data()[0], U8(7));
                    second.release();
                  },

                  [&](Negotiation::Status) { EXPECT(False); });
            },

            [&](Negotiation::Status) { EXPECT(False); });
      },

      [&] { EXPECT(False); });
  EXPECT_EQ(counts.acquisitions, counts.released);
}

VALIDATION_TEST(Retention, scoped_and_refused) {
  Observations counts;
  Discovery source = Discovery(counts);
  const auto weak = Abstract(Discovery::resolve(&source));
  EXPECT_EQ(weak.get_data()[0], U8(7));

  const Negotiation::Status answers[] = {
    Negotiation::Status::Unknown, Negotiation::Status::Rejected};

  weak.bind<Capabilities::Retain>().visit(
      [&](Capabilities::Retain request) {
        for (const auto answer : answers) {
          source.answer = answer;
          request.retain().visit(
              [&](Policies::Retained) { EXPECT(False); },
              [&](Negotiation::Status failure) {
                EXPECT(Negotiation::Status(failure) == answer);
              });
        }
      },

      [&](Negotiation::Status) { EXPECT(False); });
  EXPECT_EQ(counts.acquisitions, Count(0));
  EXPECT_EQ(counts.released, Count(0));
}

// The provider returns a Retained record with a release function and an
// incomplete Abstract table. The C++ `retain` method calls that release
// function with the returned context before reporting Rejected.
VALIDATION_TEST(Retention, malformed_answer_return) {
  struct Provider {
    Count releases = 0;

    static auto release(void* context) -> void {
      ++Data::cast<Provider>(context)->releases;
    }

    static auto retain(void* context, ttx_retained* output)
        -> ttx_negotiation_status {
      static const ttx_retained_ops malformed = {{}, release};
      *output = {context, &malformed};
      return TTX_NEGOTIATION_SATISFIED;
    }
  } provider;

  // None's Abstract functions ignore context, so this Retain interface can
  // reuse them with the fixture's Provider pointer. Its retain function returns
  // a different table with missing Abstract operations to exercise cleanup.
  const ttx_retain_ops operations = {*ttx_none().operations, Provider::retain};
  const Capabilities::Retain request({&provider, &operations});
  ASSERT(Capabilities::Retain::accept(request.get_abi()));
  request.retain().visit(
      [&](Policies::Retained) { EXPECT(False); },
      [&](Negotiation::Status failure) {
        EXPECT(failure == Negotiation::Status::Rejected);
      });

  EXPECT_EQ(provider.releases, Count(1));
}
