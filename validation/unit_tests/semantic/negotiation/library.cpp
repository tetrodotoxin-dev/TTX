// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/support/library.hpp"

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
          library, Core::NullTerminated::to_view(
                       TTX_LIBRARY_ENTRY, sizeof(TTX_LIBRARY_ENTRY) - 1)));
  ttx_binding_status admission = TTX_BINDING_UNKNOWN;
  struct Admission {
    static auto bind(void*, perimortem_uuid, ttx_storage)
        -> ttx_binding_status {
      return TTX_BINDING_UNKNOWN;
    }

    static auto supports(void* context, perimortem_uuid) -> ttx_binding_status {
      return *static_cast<ttx_binding_status*>(context);
    }
  };
  const ttx_semantic_query host = {
    &admission, Admission::bind, Admission::supports};
  Count calls = 0;
  auto function = [&](Query query) {
    ++calls;
    query.bind<Ttx::Concept::Abstract>().visit(
        [&](Ttx::Concept::Abstract subject) {
          EXPECT(subject.get_data() == "subject"_view);
        },
        [&](Binding::Failure) { EXPECT(False); });
    return Binding::Status::Rejected;
  };
  const Callback callback(function);
  EXPECT_EQ(entry(host, callback.get_abi()), TTX_BINDING_UNKNOWN);
  admission = TTX_BINDING_REJECTED;
  EXPECT_EQ(entry(host, callback.get_abi()), TTX_BINDING_REJECTED);
  EXPECT_EQ(calls, Count(0));
  admission = TTX_BINDING_SATISFIED;
  EXPECT_EQ(entry(host, callback.get_abi()), TTX_BINDING_REJECTED);
  EXPECT_EQ(calls, Count(1));
}
