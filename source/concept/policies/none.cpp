// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/policies/none.hpp"

#include "perimortem/core/null_terminated.hpp"

using namespace Ttx::Concept;
using namespace Ttx::Semantic::Negotiation;
using namespace Perimortem;

class NoneProvider {
 public:
  auto get_data() const -> Core::View::Bytes { return "None"_view; }
  auto supports(System::Uuid id) const -> Binding::Status {
    return id == Policies::None::contract_id ? Binding::Status::Satisfied
                                             : Binding::Status::Unknown;
  }
  auto bind_interface(System::Uuid id, Ttx::Data::Form::Storage target) const
      -> Binding::Status {
    if (id == Policies::None::contract_id) {
      return Binding::provide<Policies::None>(
          Abstract::provide(*this).get_abi(), target);
    }
    return Binding::Status::Unknown;
  }
  auto resolve_concept(Core::View::Bytes) const -> Abstract {
    return Policies::None::get_none();
  }
};

auto Policies::None::get_none() -> None {
  static const NoneProvider subject;
  return None(Abstract::provide(subject).get_abi());
}

PERIMORTEM_C ttx_abstract ttx_none(void) {
  return Policies::None::get_none().get_abi();
}
