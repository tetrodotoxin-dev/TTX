// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "toolchain/memory/lifetime.hpp"
#include "toolchain/validation/test.hpp"

#include "perimortem/core/data.hpp"
#include "perimortem/memory/allocator/global.hpp"

#include "ttx/graph/capabilities/create.hpp"
#include "ttx/graph/capabilities/retain.hpp"
#include "ttx/graph/policies/constant.hpp"
#include "ttx/graph/policies/none.hpp"
#include "ttx/graph/policies/unknown.h"
#include "validation/tests/graph/fixtures/value.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Toolchain::Memory;
using namespace Toolchain::Validation;
using namespace Ttx::Graph;
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

  static auto read(void* context) -> U8 {
    return Data::cast<Retained>(context)->value;
  }

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const Identity::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Policies::Retained::contract_id ||
                   contract == Capabilities::Retain::contract_id ||
                   contract == Capabilities::Create::contract_id ||
                   contract == Policies::Constant::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
      return TTX_NEGOTIATION_SATISFIED;
    }

    auto& subject = *Data::cast<Retained>(context);
    ++subject.counts.binds;

    const Identity::Uuid contract(id);
    if (contract == Policies::Retained::contract_id) {
      Policies::Retained::Api api = {context, &retained_operations};
      *Data::cast<Policies::Retained::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Capabilities::Retain::contract_id) {
      Capabilities::Retain::Api api = {context, &retain_operations};
      *Data::cast<Capabilities::Retain::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Capabilities::Create::contract_id) {
      Capabilities::Create::Api api = {context, &create_operations};
      *Data::cast<Capabilities::Create::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Abstract::contract_id ||
        contract == Policies::Constant::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    return TTX_NEGOTIATION_UNKNOWN;
  }

  static auto get_abstract(void* context) -> ttx_abstract {
    return {context, &abstract_operations};
  }

  static auto lookup(void* context, perimortem_view_bytes route)
      -> ttx_abstract {
    return ttx_unknown();
  }

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

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
    callback(callback_context, get_abstract(context));
    return TTX_NEGOTIATION_SATISFIED;
  }

  static auto release(void* context) -> ttx_release_status {
    auto& subject = *Data::cast<Retained>(context);
    ++subject.counts.released;
    if (!--subject.references) {
      U8* storage = Data::cast<U8>(&subject);
      subject.~Retained();
      Allocator::Global::release(storage);
    }

    return TTX_RELEASE_SUCCESS;
  }

  static constexpr ttx_abstract_ops abstract_operations = {
    supports, bind, lookup, visit, state};

  static constexpr ttx_retained_ops retained_operations = {
    get_abstract, release};

  static constexpr ttx_retain_ops retain_operations = {get_abstract, retain};

  static constexpr ttx_create_ops create_operations = {get_abstract, create};
};

static_assert(alignof(Retained) <= Allocator::Global::allocation_alignment);

class Discovery {
 public:
  Observations& counts;
  U8 value = 7;
  Negotiation::Status answer = Negotiation::Status::Satisfied;

  static auto read(void* context) -> U8 {
    return Data::cast<Discovery>(context)->value;
  }

  static auto state(void* context) -> ttx_abstract_state {
    return TTX_ABSTRACT_PRESENT;
  }

  static auto supports(void* context, perimortem_uuid id)
      -> ttx_negotiation_status {
    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    const Identity::Uuid contract(id);
    return contract == Abstract::contract_id ||
                   contract == Capabilities::Retain::contract_id
               ? TTX_NEGOTIATION_SATISFIED
               : TTX_NEGOTIATION_UNKNOWN;
  }

  static auto bind(void* context, perimortem_uuid id, void* output)
      -> ttx_negotiation_status {
    if (id.high == TTX_NONE_ID_HIGH &&
        (id.low == TTX_NONE_ID_LOW || id.low == TTX_UNKNOWN_ID_LOW)) {
      return TTX_NEGOTIATION_REJECTED;
    }

    if (Identity::Uuid(id) == Validation::Value::contract_id) {
      *Data::cast<value_api>(output) = {context, read};
      return TTX_NEGOTIATION_SATISFIED;
    }

    const Identity::Uuid contract(id);
    if (contract == Capabilities::Retain::contract_id) {
      Capabilities::Retain::Api api = {context, &operations};
      *Data::cast<Capabilities::Retain::Api>(output) = api;
      return TTX_NEGOTIATION_SATISFIED;
    }

    if (contract == Abstract::contract_id) {
      Abstract::Api api = get_abstract(context);
      *Data::cast<Abstract::Api>(output) = api;
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

  static auto visit(void* context, ttx_graph_visitor callback) -> void {}

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

  static constexpr ttx_abstract_ops graph_operations = {
    supports, bind, lookup, visit, state};

  static constexpr ttx_retain_ops operations = {get_abstract, retain};
};

VALIDATION_TEST(Retention, independent_context) {
  Observations counts;
  Storage::Option<Policies::Retained> retained;
  {
    Discovery source = Discovery(counts);
    const auto weak = Abstract(Discovery::get_abstract(&source));
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

        [&]() { EXPECT(False); });

    source.value = 12;
    retained.visit(
        [&](Policies::Retained acquired) {
          EXPECT_EQ(
              acquired.get_abstract().bind<Validation::Value>().visit(
                  [](Validation::Value value) { return value.read(); },
                  [&] {
                    EXPECT(False);
                    return U8(0);
                  }),

              U8(7));
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
                  EXPECT_EQ(
                      answer.bind<Validation::Value>().visit(
                          [](Validation::Value value) { return value.read(); },
                          [&] {
                            EXPECT(False);
                            return U8(0);
                          }),

                      U8(7));
                };

                EXPECT(
                    create.create(
                        Policies::None::get_none().get_abstract(), callback) ==
                    Negotiation::Status::Satisfied);
              }

              EXPECT_EQ(counts.binds, binds);
            },

            [&]() { EXPECT(False); });
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
                    EXPECT_EQ(
                        second.get_abstract().bind<Validation::Value>().visit(
                            [](Validation::Value value) {
                              return value.read();
                            },

                            [&] {
                              EXPECT(False);
                              return U8(0);
                            }),

                        U8(7));
                    second.release();
                  },

                  [&](Negotiation::Status) { EXPECT(False); });
            },

            [&]() { EXPECT(False); });
      },

      [&] { EXPECT(False); });
  EXPECT_EQ(counts.acquisitions, counts.released);
}

VALIDATION_TEST(Retention, scoped_and_refused) {
  Observations counts;
  Discovery source = Discovery(counts);
  const auto weak = Abstract(Discovery::get_abstract(&source));
  EXPECT_EQ(
      weak.bind<Validation::Value>().visit(
          [](Validation::Value value) { return value.read(); },
          [&] {
            EXPECT(False);
            return U8(0);
          }),

      U8(7));

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

      [&]() { EXPECT(False); });
  EXPECT_EQ(counts.acquisitions, Count(0));
  EXPECT_EQ(counts.released, Count(0));
}

VALIDATION_TEST(Retention, present_bindings) {
  Observations counts;
  Discovery discovery{counts};
  Retained retained{counts, 1, 7};
  const ttx_abstract graphs[] = {
    Discovery::get_abstract(&discovery),
    Retained::get_abstract(&retained),
  };

  const perimortem_uuid ids[] = {
    {TTX_NONE_ID_HIGH, TTX_NONE_ID_LOW},
    {TTX_UNKNOWN_ID_HIGH, TTX_UNKNOWN_ID_LOW},
  };

  for (const auto api : graphs) {
    EXPECT_EQ(api.operations->get_state(api.context), TTX_ABSTRACT_PRESENT);
    for (const auto id : ids) {
      EXPECT_EQ(
          api.operations->supports(api.context, id), TTX_NEGOTIATION_REJECTED);

      ttx_abstract output{};
      EXPECT_EQ(
          api.operations->bind(api.context, id, &output),
          TTX_NEGOTIATION_REJECTED);
    }
  }
}
