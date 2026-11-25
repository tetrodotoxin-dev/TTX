/- Copyright (c) 2023-present Matt Kaes and contributors -/

namespace TTX

abbrev Bytes := List UInt8

/-- The first layout model includes the nested signatures of callable fields. -/
inductive Shape where
  | scalar (bits : Nat)
  | opaquePointer (bits : Nat)
  | pointer (bits : Nat) (target : Shape)
  | record (extent alignment : Nat) (fields : List (Nat × Shape))
  | callable (convention : Nat) (arguments : List Shape) (result : Shape)

structure Layout where
  extent : Nat
  alignment : Nat
  shape : Shape

/-- The consumer knows a canonical destination form and how it decodes. -/
structure CanonicalForm (decode : Bytes → Option Layout) where
  descriptor : Bytes
  layout : Layout
  describesLayout : decode descriptor = some layout

/-- Bytes model accessible storage supplied for the transfer. -/
structure Region where
  address : Nat
  bytes : Bytes

structure Storage where
  descriptor : Bytes
  region : Region

def Region.fits (region : Region) (layout : Layout) : Bool :=
  decide (layout.extent ≤ region.bytes.length) &&
    (layout.extent == 0 ||
      (region.address != 0 && 0 < layout.alignment &&
        region.address % layout.alignment == 0))

/-- Transfer compares the form before using its geometry. The returned bytes
    model a successful copy, including any untouched destination capacity. -/
def transfer {decode : Bytes → Option Layout} (expected : CanonicalForm decode)
    (source : Storage) (destination : Region) : Option Bytes :=
  if source.descriptor == expected.descriptor &&
      source.region.fits expected.layout && destination.fits expected.layout then
    let extent := expected.layout.extent
    some (source.region.bytes.take extent ++ destination.bytes.drop extent)
  else
    none

/-- A successful transfer copies one represented value into the consumer's
    storage. The source descriptor decodes to `provided`, and the consumer's
    canonical form carries the decoding of its own descriptor. Success requires
    identical descriptor bytes, so the shared decoder gives both the same layout.

    Both regions have room for the represented extent. For a nonzero extent,
    both addresses are nonzero and satisfy the layout's positive alignment.
    Every byte within the extent comes from the source, including padding.
    Every byte beyond it comes from the original destination. At zero extent
    the entire destination is preserved.

    The model takes `decode` and the region bytes as inputs. Connecting them
    to a native transfer requires a decoder for the canonical storage format,
    readable descriptors and source storage, and writable destination storage.
    The provider must describe the actual payload faithfully. Copying a function
    pointer preserves its bits. Calling it also requires an implementation
    with the described ABI and access to its code and context through use. -/
theorem successfulTransferPreservesLayoutAndBytes {decode : Bytes → Option Layout}
    (expected : CanonicalForm decode) (source : Storage) (destination : Region)
    (provided : Layout) (result : Bytes)
    (description : decode source.descriptor = some provided)
    (copied : transfer expected source destination = some result) :
    provided = expected.layout ∧
      result.take expected.layout.extent = source.region.bytes.take expected.layout.extent ∧
      result.drop expected.layout.extent = destination.bytes.drop expected.layout.extent := by
  sorry

/-- A consumer can refuse a record whose outer size and alignment match its
    own. If the shared decoder gives the source and destination different
    layouts, their descriptor bytes must differ. Transfer compares those bytes
    before consulting either region's geometry and returns `none`.

    A nested operation can occupy the same pointer slot while changing its
    result, arguments, or calling convention. This theorem covers each such
    difference that `decode` preserves in `Shape`. Establishing which native
    signatures the canonical format distinguishes requires evidence about
    its encoder and decoder.

    Equal decoded layouts alone leave descriptor equality and region fit open.
    On refusal, `none` supplies no result bytes. The native transfer contract
    also specifies a failure status and preserves every destination byte.
    Those observations require a native destination and a status result. -/
theorem transferRejectsDifferentLayout {decode : Bytes → Option Layout}
    (expected : CanonicalForm decode) (source : Storage) (destination : Region)
    (provided : Layout) (description : decode source.descriptor = some provided)
    (different : provided ≠ expected.layout) :
    transfer expected source destination = none := by
  sorry

end TTX
