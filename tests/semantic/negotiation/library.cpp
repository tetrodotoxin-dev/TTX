// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/library.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "toolchain/validation/unit_test.hpp"
#include "ttx/concept/abstract.hpp"
#include "ttx/semantic/negotiation/library.hpp"

using namespace Perimortem;
using namespace Ttx::Semantic::Negotiation;

static Toolchain::Validation::Harness NativeEntry = {.name = "TTX::Library"};

VALIDATION_TEST(NativeEntry, scoped_negotiation) {
  const auto library = Validation::open_library("libabstract_subject.so"_view);
  const auto entry =
      reinterpret_cast<ttx_library_entry>(Validation::find_symbol(
          library, Core::NullTerminated::to_view(TTX_LIBRARY_ENTRY)));
  ttx_binding_status admission = TTX_BINDING_UNKNOWN;
  const ttx_semantic_query host = ttx_semantic_query(
      &admission,
      [](const void*, perimortem_uuid, ttx_storage) -> ttx_binding_status {
        return TTX_BINDING_UNKNOWN;
      },
      [](const void* source, perimortem_uuid) {
        return *static_cast<const ttx_binding_status*>(source);
      });
  Count received = 0;
  auto receive = [&](Query query) {
    ++received;
    query.bind<Ttx::Concept::Abstract>().visit(
        [&](Ttx::Concept::Abstract subject) {
          EXPECT(subject.get_data() == "subject"_view);
        },
        [&](Binding::Failure) { EXPECT(False); });
    return Binding::Status::Rejected;
  };
  const Receiver receiver(receive);
  EXPECT_EQ(entry(host, receiver.get_abi()), TTX_BINDING_UNKNOWN);
  admission = TTX_BINDING_REJECTED;
  EXPECT_EQ(entry(host, receiver.get_abi()), TTX_BINDING_REJECTED);
  EXPECT_EQ(received, Count(0));
  admission = TTX_BINDING_SATISFIED;
  EXPECT_EQ(entry(host, receiver.get_abi()), TTX_BINDING_REJECTED);
  EXPECT_EQ(received, Count(1));
}
