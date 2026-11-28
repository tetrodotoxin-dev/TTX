// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/native.hpp"

#include "toolchain/validation/test.hpp"

#include "ttx/data/transfer.hpp"

using namespace Perimortem::Core;
using namespace Toolchain::Validation;
using namespace Ttx::Data;

static Harness NativeForms = {.name = "TTX::Data::Form::Native"};

// Identity distinguishes C++ types while leaving their declared fields equal.
// Result and Argument independently change one operation's signature. The
// recursive return must preserve each record's complete description.
template <typename Result, typename Argument, Count identity>
struct Recursive {
  void* context;
  Recursive (*next)(void* context);
  Result (*read)(void* context, Argument amount);
};

template <>
class Form::Native<Recursive<U32, U32, 0>> {
 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

template <>
class Form::Native<Recursive<U32, U32, 1>> {
 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

template <>
class Form::Native<Recursive<U64, U32, 0>> {
 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

template <>
class Form::Native<Recursive<U32, U64, 0>> {
 public:
  static const Schema schema;
  static constexpr Schema::Reference reference = schema;
};

template <typename Record>
static consteval auto describe() -> Form::Schema {
  static constexpr Storage::Vector<Form::Schema::Position, 3> fields = {{
    TTX_DATA_MEMBER(Record, context),
    TTX_DATA_MEMBER(Record, next),
    TTX_DATA_MEMBER(Record, read),
  }};

  return Form::Schema::composite(
      fields.get_view(), sizeof(Record), alignof(Record));
}

inline constexpr Form::Schema Form::Native<Recursive<U32, U32, 0>>::schema =
    describe<Recursive<U32, U32, 0>>();
inline constexpr Form::Schema Form::Native<Recursive<U32, U32, 1>>::schema =
    describe<Recursive<U32, U32, 1>>();
inline constexpr Form::Schema Form::Native<Recursive<U64, U32, 0>>::schema =
    describe<Recursive<U64, U32, 0>>();
inline constexpr Form::Schema Form::Native<Recursive<U32, U64, 0>>::schema =
    describe<Recursive<U32, U64, 0>>();

static auto read(void* context, U32 amount) -> U32 {
  return *Data::cast<U32>(context) + amount;
}

static auto next(void* context) -> Recursive<U32, U32, 0> {
  return {context, next, read};
}

VALIDATION_TEST(NativeForms, array_parameter_bounds) {
  using Adjusted = U32 (*)(const U32[4]);
  using Elements = U32 (*)(const U32*);
  using Fixed = U32 (*)(const U32(*)[4]);
  using Mutable = U32 (*)(U32(*)[4]);
  using Short = U32 (*)(const U32(*)[3]);

  static_assert(__is_same(Adjusted, Elements));
  static constexpr auto& fixed =
      Form::Compiled<Form::Native<Fixed>::reference>::get_representation();
  static_assert(fixed.compatible(
      Form::Compiled<Form::Native<Mutable>::reference>::get_representation()));

  // Lean reads one argument from both System V AMD64 fixtures. The fixed
  // pointer leads to four U32 elements. The adjusted pointer describes a U32.
  if constexpr (
      Form::Callable::Convention::Native ==
      Form::Callable::Convention::SystemVAMD64) {
    static constexpr U8 fixed_bytes[] = {
      0x11, 0x80, 0x80, 0x00, 0x82, 0x21, 0x00, 0x01, 0x03, 0x00, 0x01, 0x01,
      0x84, 0x02, 0x00, 0x01, 0x11, 0x40, 0x00, 0x01, 0x03, 0x10, 0x00, 0x04,
    };

    static constexpr U8 element_bytes[] = {
      0x11, 0x80, 0x80, 0x00, 0x82, 0x21, 0x00, 0x01,
      0x03, 0x00, 0x01, 0x01, 0x83, 0x00, 0x00, 0x01,
    };

    static constexpr auto& adjusted =
        Form::Compiled<Form::Native<Adjusted>::reference>::get_representation();
    EXPECT(fixed.compatible(
        Form::Representation(fixed_bytes, sizeof(fixed_bytes))));
    EXPECT(adjusted.compatible(
        Form::Representation(element_bytes, sizeof(element_bytes))));
  }

  Fixed read = [](const U32(*values)[4]) -> U32 { return (*values)[3]; };
  const auto source = Form::Storage::from(read);
  Fixed received = nullptr;
  ASSERT(
      Transfer::copy(source, Form::Storage::from(received)) == Status::Success);
  ASSERT(received);

  const U32 values[4] = {10, 20, 30, 40};
  EXPECT_EQ(received(&values), U32(40));

  Adjusted elements = nullptr;
  Short three = nullptr;
  EXPECT(
      Transfer::copy(source, Form::Storage::from(elements)) ==
      Status::Incompatible);
  EXPECT(
      Transfer::copy(source, Form::Storage::from(three)) ==
      Status::Incompatible);
  EXPECT(!elements && !three);
}

VALIDATION_TEST(NativeForms, recursive_signature_agreement) {
  static constexpr auto& original = Form::Compiled<
      Form::Native<Recursive<U32, U32, 0>>::reference>::get_representation();
  static constexpr auto& equivalent = Form::Compiled<
      Form::Native<Recursive<U32, U32, 1>>::reference>::get_representation();
  static constexpr auto& changed_result = Form::Compiled<
      Form::Native<Recursive<U64, U32, 0>>::reference>::get_representation();
  static constexpr auto& changed_argument = Form::Compiled<
      Form::Native<Recursive<U32, U64, 0>>::reference>::get_representation();

  static_assert(
      sizeof(Recursive<U32, U32, 0>) == sizeof(Recursive<U64, U32, 0>));
  static_assert(
      sizeof(Recursive<U32, U32, 0>) == sizeof(Recursive<U32, U64, 0>));
  static_assert(original.compatible(equivalent));
  static_assert(!original.compatible(changed_result));
  static_assert(!original.compatible(changed_argument));

  U32 value = 41;
  auto input = next(&value);
  Recursive<U32, U32, 0> output{};
  ASSERT(
      Transfer::copy(Form::Storage::from(input), Form::Storage::from(output)) ==
      Status::Success);

  const auto returned = output.next(output.context);
  EXPECT_EQ(returned.read(returned.context, 1), U32(42));

  Recursive<U64, U32, 0> wrong_result{};
  EXPECT(
      Transfer::copy(
          Form::Storage::from(input), Form::Storage::from(wrong_result)) ==
      Status::Incompatible);
  EXPECT(!wrong_result.context && !wrong_result.next && !wrong_result.read);

  Recursive<U32, U64, 0> wrong_argument{};
  EXPECT(
      Transfer::copy(
          Form::Storage::from(input), Form::Storage::from(wrong_argument)) ==
      Status::Incompatible);
  EXPECT(
      !wrong_argument.context && !wrong_argument.next && !wrong_argument.read);
}
