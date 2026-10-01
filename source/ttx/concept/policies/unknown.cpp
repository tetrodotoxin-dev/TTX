// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/unknown.hpp"

#include "perimortem/core/null_terminated.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
using namespace Perimortem;

class UnknownProvider {
 public:
  auto get_data() const -> Core::View::Bytes { return "Unknown"_view; }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Policies::Unknown::contract_id ? Binding::Status::Satisfied
                                                : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Ttx::Data::Form::Storage target)
      -> Binding::Status {
    if (id == Policies::Unknown::contract_id) {
      return Binding::provide<Policies::Unknown>(
          Abstract::provide(*this).get_abi(), target);
    }
    return Binding::Status::Unknown;
  }
  auto resolve_concept(Core::View::Bytes) const -> Abstract {
    return Policies::Unknown::get_unknown();
  }
};

auto Policies::Unknown::get_unknown() -> Unknown {
  static UnknownProvider subject;
  return Unknown(Abstract::provide(subject).get_abi());
}

PERIMORTEM_C ttx_abstract ttx_unknown(void) {
  return Policies::Unknown::get_unknown().get_abi();
}
