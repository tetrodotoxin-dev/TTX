// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/capabilities/import.h"
#include "ttx/concept/policies/none.h"

namespace Ttx::Concept::Capabilities {

// Exposes the capability to interpret data described by the supplied
// Representation and produce an Abstract projection. The provider checks the
// representation before interpreting the input. If it can produce a valid
// projection, visit invokes the receiver once with the Abstract representing
// the imported graph and returns Satisfied after the callback finishes.
//
// The receiver is a callable passed by reference and invoked with an Abstract.
// Its captured state carries the consumer's observations. visit returns the
// provider's status and discards any value returned by the receiver. The
// input, Representation and receiver remain available until visit returns. The
// imported graph is available during the callback, where the consumer can
// inspect it, copy data or negotiate retained access through the graph's
// capabilities.
//
// A visit returning Unknown or Rejected guarantees that the receiver was never
// invoked. Unknown leaves the import request undetermined. Rejected explicitly
// refuses it. provide adapts a native owner's visit operation. An owner that
// omits visit supplies the default import, which passes None to the receiver
// and returns Satisfied.
class Import : public Abstract {
 public:
  static constexpr auto contract_id =
      Perimortem::System::Uuid(TTX_IMPORT_ID_HIGH, TTX_IMPORT_ID_LOW);
  using Api = ttx_import;
  using Operations = ttx_import_ops;
  static auto accept(Api value) -> Bool {
    return value.operations && value.operations->visit &&
           Abstract::accept(
               ttx_abstract(value.source, &value.operations->abstract));
  }
  explicit constexpr Import(Api value)
      : Abstract(value.source, value.operations->abstract) {}
  constexpr auto get_abi() const -> Api {
    const auto value = Abstract::get_abi();
    return Api(
        value.source, reinterpret_cast<const Operations*>(value.operations));
  }

  template <typename Receiver>
  auto visit(
      const void* input,
      const Data::Form::Representation& representation,
      Receiver& receiver) const -> Semantic::Negotiation::Binding::Status {
    const auto api = get_abi();
    return static_cast<Semantic::Negotiation::Binding::Status>(
        api.operations->visit(
            api.source, input, &representation, &receiver,
            [](void* state, ttx_abstract subject) {
              (*static_cast<Receiver*>(state))(Abstract(subject));
            }));
  }

  template <typename Owner>
    requires(!__is_base_of(Abstract, Owner))
  static auto provide(const Owner& owner) -> Import {
    static const Operations operations = Operations(
        *Abstract::provide(owner).get_abi().operations,
        [](const void* source, const void* input,
           const ttx_representation* representation, void* receiver,
           void (*receive)(void*, ttx_abstract)) -> ttx_binding_status {
          auto observe = [&](Abstract subject) {
            receive(receiver, subject.get_abi());
          };
          if constexpr (requires(const Owner& provider) {
                          provider.visit(input, *representation, observe);
                        }) {
            return static_cast<ttx_binding_status>(
                static_cast<const Owner*>(source)->visit(
                    input, *representation, observe));
          } else {
            receive(receiver, ttx_none());
            return TTX_BINDING_SATISFIED;
          }
        });
    return Import(Api(&owner, &operations));
  }
};

}  // namespace Ttx::Concept::Capabilities

TTX_DATA_RECORD(
    ttx_import_ops,
    TTX_DATA_MEMBER(ttx_import_ops, abstract),
    TTX_DATA_MEMBER(ttx_import_ops, visit));
TTX_DATA_RECORD(
    ttx_import,
    TTX_DATA_MEMBER(ttx_import, source),
    TTX_DATA_MEMBER(ttx_import, operations));
