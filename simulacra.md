# Simulacra

This document is the single exception to TTX's practice of keeping context
beside the contract that owns it. Simulacra has no local owner: it is an
emergent property of TTX's composition model. Naming it provides useful
shorthand for reasoning about advanced interoperability without introducing
another contract, policy, capability or graph node.

This is secondary reading. It assumes familiarity with TTX's
[composable interfaces](README.md#composable-interfaces),
[semantic graph](README.md#semantic-graph) and
[`Abstract`](source/ttx/graph/abstract.h). Those models are not restated here.
Understanding Simulacra is not required to use TTX, but it substantially
shortens the path to using its open composition model productively.

## An emergent property

Simulacra names the substitution that emerges when distinct systems supply the
same semantic contracts. The systems can use unrelated runtimes, storage,
contexts and operations. TTX observes the contracts they negotiate rather than
assigning meaning to those implementation choices.

No provider declares itself a Simulacrum. There is no Simulacra UUID, marker,
base class or detection operation. Adding one would replace an emergent
relationship with one more interface that a provider could imitate. The
relationship exists only between observations of systems through contracts
they share.

This document calls the system being substituted the Original and the
substituting system a simulacrum. These roles are relative. A system can be an
Original in one comparison and a simulacrum in another; neither role grants
authority over the semantic graph.

## The semantic agreement

Simulacra is relative to one consumer observation. The Original supplies a
larger semantic domain, while the consumer constrains the portion it needs.
Their agreement, rather than the Original's complete behavior, determines
which substitutes that observation can admit.

The mathematical names used below remain deliberately explicit:

| Name | Meaning |
| --- | --- |
| `Original` | The provider whose behavior supplies the comparison. |
| `Domain(System)` | A provider's complete semantic meaning. |
| `Consumer` | The observations and outcomes required by one consumer. |
| $A_{\mathrm{con}}$ | The closed agreement between Consumer and Original. |
| `Candidates` | Provider behaviors preserving the current agreement. |
| $F_{\mathrm{con}}$ | Non-Perfect candidate behavior left open by the agreement. |
| `Universe` | The semantic observations used to test complete equivalence. |

In plain language, the Consumer selects meaning from the Original's Domain.
Closure includes the consequences of that selection. Candidates preserve the
closed agreement, and $F_{\mathrm{con}}$ contains the non-Perfect alternatives
that remain. The equations below make each step precise.

The agreement for one Consumer is:

```math
A_{\mathrm{con}} = \operatorname{Closure}
\left(\operatorname{Consumer} \cap \operatorname{Domain}(\operatorname{Original})\right)
```

The intersection contains the semantic premises shared by the Consumer and
the Original. An incompatible requirement establishes no agreement.
`Closure` adds everything those premises necessarily imply during the same
observation:

```math
\operatorname{Closure}(X) =
\{\varphi \mid X \text{ semantically entails } \varphi\}
```

`Closure` is reasoning notation rather than a TTX capability or an algorithm a
provider executes. A provider need not enumerate the complete closure. It must
preserve the consequences exposed through the observation it supplies.

Closure prevents a consumer from selecting one promise while discarding a
consequence of that promise. It is extensive, monotonic and idempotent:

```math
X \subseteq \operatorname{Closure}(X)
```

```math
X \subseteq Y \Rightarrow
\operatorname{Closure}(X) \subseteq \operatorname{Closure}(Y)
```

```math
\operatorname{Closure}(\operatorname{Closure}(X)) =
\operatorname{Closure}(X)
```

The candidate set makes the agreement concrete:

```math
\operatorname{Candidates}(A_{\mathrm{con}}) =
\left\{
\operatorname{Candidate}
\;\middle|\;
\operatorname{Closure}
\left(
\operatorname{Consumer} \cap
\operatorname{Domain}(\operatorname{Candidate})
\right)
\supseteq A_{\mathrm{con}}
\right\}
```

A candidate can establish more Consumer-visible meaning than the Original, but
it cannot establish less than their agreement. It is admitted as a simulacrum
for that observation when it belongs to this set, occupies a different native
address from the Original and reenters through a different semantic graph
location. The latter differences establish that substitution occurred; they
do not weaken the shared meaning.

An empty candidate set means the requirements are inconsistent. An agreement
without any semantic witness establishes no substitution relationship at all.
These cases are distinct: incompatibility refuses a comparison, while an empty
observation proves nothing about one.

## Freedom and perfection

Two systems are equivalent across the semantic Universe when no contract
observation in that Universe distinguishes their committed meaning. Private
addresses, operation tables and storage do not belong to the Universe unless
an explicit contract gives them semantic meaning. This document writes that
equivalence as:

```math
\operatorname{Candidate}
\equiv_{\mathrm{Universe}}
\operatorname{Original}
```

The Consumer's remaining freedom is the set of admitted candidates that are
not equivalent to the Original:

```math
F_{\mathrm{con}} =
\left\{
\operatorname{Candidate}
\in \operatorname{Candidates}(A_{\mathrm{con}})
\;\middle|\;
\operatorname{Candidate}
\not\equiv_{\mathrm{Universe}}
\operatorname{Original}
\right\}
```

This definition covers more than unobserved contracts. An agreement such as
`size >= 1` observes the size but still admits several meanings. The remaining
values belong to $F_{\mathrm{con}}$ even though the Consumer mentioned that
dimension. When every constrained value is exact and independent, freedom can
be pictured more simply as the unobserved portion of the Original's Domain.

There is no separate removal region. A difference that contradicts
$A_{\mathrm{con}}$ is invalid. Every other difference belongs to
$F_{\mathrm{con}}$. This is why a constrained consumer can admit a projection
of a richer system without claiming that the projection preserves the entire
Original.

A particular candidate is a Perfect Simulacrum when:

```math
\operatorname{Candidate}
\equiv_{\mathrm{Universe}}
\operatorname{Original}
```

The agreement instead forces perfection when:

```math
F_{\mathrm{con}} = \varnothing
```

These statements are deliberately different. A Perfect candidate can exist
when the Consumer leaves freedom; that observation merely cannot prove its
perfection. Empty freedom means every candidate admitted by the Consumer must
be Perfect.

An Original is Axiomatically Perfect when every consistent consumer agreement
that identifies that Original forces empty freedom. This definition excludes
an observation that sees only an incidental contract. Observing Constant from
None compares Constant behavior; it does not by itself identify the result as
None.

Perfect does not mean preferable. Requiring Perfect Simulacra makes the
complete semantic Universe part of the consumer's dependency. Adding one
independent capability then breaks substitution much as changing a closed
typed interface would. TTX's productive composition model depends on
consumers constraining the semantic surface they need rather than demanding
perfection.

## Transactions, outcomes and state

The agreement belongs to one observation. A successful binding commits the
meaning selected by its contract UUID for that observation. A simulacrum can
implement the work differently and can produce different internal side
effects, but it cannot change the meaning of the result it committed.

Those side effects can affect later observations. Unless a negotiated policy
states otherwise, the Original and simulacrum can evolve differently after
answering the same request. Simulacra therefore establishes transactional
substitution rather than permanent equality between systems. Generated
records, delegated implementations and temporary materializations all fit
this rule.

Unknown is an observed outcome rather than unused freedom. Once the Consumer
observes Unknown, $A_{\mathrm{con}}$ records that the provider left the request
unsettled. Rejected similarly records a refusal, while None records an explicit
graph dead end. A question the Consumer never asks remains free; an answer of
Unknown does not become unasked.

Support and binding statuses need not follow the same path unless the Consumer
constrains both. A simulacrum can satisfy a request the Original leaves Unknown
or Rejected when that outcome lies in $F_{\mathrm{con}}$, for example by adding
knowledge, authority or conversion. Every Satisfied result must still have the
requested contract's meaning.

Stateful systems also need not contain equal raw values. Their states can use a
different representation or correspondence as long as each committed
operation preserves the semantic relationship required by
$A_{\mathrm{con}}$. A Consumer that requires a particular value or history
adds that requirement to its agreement.

## Closed axioms

None and Unknown test the boundary of extension. Their defining behavior is
the complete meaning assigned to the two axiomatic Graph results by
[`Abstract`](source/ttx/graph/abstract.h): classification, traversal,
visitation, reserved policies and the outcome of every other UUID request.

Once a Consumer identifies one of these graph results, semantic closure fixes
its complete behavior and leaves $F_{\mathrm{con}}$ empty. Every simulacrum
admitted as None or Unknown is therefore observationally Perfect, even though
it can use a different context and operation table. None and Unknown can never
substitute for each other because their classifications and negotiation
outcomes disagree.

A compact encoding can still store either result as a small tag. Rehydrating
that tag must recover the complete axiom. This compresses representation
without removing semantic meaning.

## Practical uses

The model helps distinguish a deliberate projection from an accidental loss of
meaning. The following uses all constrain consumers rather than weakening the
contracts they observe.

### Packaging

A package supports a declared family of consumers rather than every possible
consumer. Let `Consumers` contain that family. The package's combined agreement
is:

```math
A_{\mathrm{pkg}} = \operatorname{Closure}
\left(
\bigcup_{\operatorname{Consumer} \in \operatorname{Consumers}}
A_{\mathrm{con}}(\operatorname{Consumer})
\right)
```

Its candidates must satisfy every individual agreement:

```math
\operatorname{Candidates}(A_{\mathrm{pkg}}) =
\bigcap_{\operatorname{Consumer} \in \operatorname{Consumers}}
\operatorname{Candidates}(A_{\mathrm{con}}(\operatorname{Consumer}))
```

Closure appears around the union because two individually closed agreements
can imply more when combined. It does not change which candidates satisfy the
union; it makes those combined consequences explicit. A candidate satisfies
the union exactly when it belongs to every individual candidate set, producing
the intersection in the second equation.

Packaging can therefore retain linkable symbols without materializing
documentation, editor data or unrelated graph structure when none of its
admitted consumers constrain those meanings. A consumer-specific rehydration
can project to its own $A_{\mathrm{con}}$. Shared package storage must retain
enough information to reconstruct $A_{\mathrm{pkg}}$. Requests outside the
declared consumer family receive the package's ordinary Unknown or Rejected
outcome rather than an inadequate simulacrum.

### Extensions and sidecars

A wrapper can preserve a typed agreement while adding metadata, a policy or a
provider-specific capability. An existing Consumer leaves the extension in
$F_{\mathrm{con}}$ and continues to admit the wrapper. A later Consumer can
constrain the extension without changing the original typed contract.

For example, a generic Buffer consumer can constrain public Buffer access while
a CUDA consumer additionally requires a CUDA interoperability capability.
Intermediate systems can transport the Buffer without understanding the
sidecar. The CUDA consumer reenters the Buffer's graph location and negotiates
the additional meaning. The typed Buffer still owns access, acquisition and
release; the Abstract owns semantic location and negotiation.

### Progressive and stable consumers

An interactive tool can use a narrow Consumer and report the agreement it has
established so far. Additional observations strengthen the agreement and
reduce the candidate set. This lets an editor name a variable, an Alias and the
comments reached along the way even when its final type remains Unknown.

A compiler has a different requirement. It can constrain the input to a
coherent snapshot before producing machine code so two compiled functions do
not observe incompatible meanings. A stabilization capability can provide that
transactional precondition without making live Type consumers require the same
closure.

### Compatibility and materialization

An older Consumer continues to admit a provider extension when the extension
does not invalidate $A_{\mathrm{con}}$. A newer Consumer can select the new
meaning explicitly. Compatibility is therefore measured against consumer
agreements rather than provider versions or operation-table addresses.

The same rule supports lazy materialization and caching. A provider only needs
to construct the meaning selected by the current Consumer, while a cache can
key a result by the agreement it actually preserves. Reusing that result for a
stronger Consumer requires proving that it satisfies the stronger agreement;
pointer or representation equality cannot supply that proof.

## Semantic disagreement is not substitution

A UUID selects a shared semantic contract only when provider and consumer
assign it the same meaning. Two systems can use the same UUID for different
ideas and remain locally well-defined. Their exchange may even be repeatable
and representation-compatible, but it does not establish a shared agreement.

This is a misunderstanding rather than an absence of behavior. A request to
"get in the car" can refer to a transport vehicle for one participant and a
toy car for another. Both interpretations are meaningful; the participants
still cannot reason together until they agree on the referent.

Matching data forms cannot repair this disagreement. Representation agreement
establishes how bytes can cross a boundary. The UUID's shared meaning
establishes what those bytes promise. A system using another meaning remains an
Original under its own model, but it is not a simulacrum of the requested
contract.

## Preserving composition boundaries

Simulacra makes representation uninhibited. None of the following independently
establishes semantic identity:

- native address;
- Abstract API-record identity;
- context pointer;
- operation-table pointer;
- API-record bytes; or
- compatible data representation.

Pointer identity and casts can remain private implementation mechanisms or
belong to a contract that explicitly selects one native ABI. They cannot
establish portable semantic identity outside that contract.

A system does not implement Simulacra, but its interfaces can preserve or
obstruct the conditions under which substitution emerges. Treating a typed
API's private address, operation table or native class as semantic identity
rejects valid providers before observing their contracts and closes the
interface around one implementation.

Moving the typed value entirely into Graph creates the symmetric error. A plain
Abstract does not own stateful access, acquisition or release. The composable
form keeps both views: an Abstract location owns negotiation and relationships,
while a typed contract owns the value's operations and resource obligations.

When later consumers need additional negotiation, the typed API can provide
`get_abstract` to reenter its supplying location. This is semantic reentry, not
recovery of a privileged native object. A side-effectful typed operation can
change provider state and therefore alter later graph observations. Reentry
lets another provider discover capabilities on that changed or extended
location without requiring an intermediate consumer to understand them.

Canonicalization and other graph consumers must likewise work through
negotiated behavior. A Consumer requiring one coherent snapshot can constrain
the provider through a stabilization capability when one exists. Consumers
that intentionally observe a live graph can use narrower agreements without
pretending that the graph remains stable between observations.

## Executable evidence

The repository validates the mechanisms from which Simulacra emerges rather
than testing for a Simulacra marker:

- [`simulacra.cpp`](validation/tests/semantic/realization/simulacra.cpp)
  exercises foreign providers, semantic and representation disagreement, and
  composed contexts and operations.
- [`composition.cpp`](validation/tests/graph/composition.cpp) exercises typed
  substitution and graph reentry through distinct contexts.
- [`abstract.cpp`](validation/tests/graph/abstract.cpp) demonstrates one
  semantic location represented by fresh API records with different operation
  tables.
- [`sentinels.cpp`](validation/tests/graph/policies/sentinels.cpp) establishes
  the closed axioms that make every valid None or Unknown simulacrum
  observationally Perfect.

These tests establish observable consequences. The relationship itself remains
emergent and cannot be made more general by giving it an interface.

## Boundary of this document

This document introduces no contract and owns no API behavior. Contract-specific
guarantees remain beside their declarations. Alias, stabilization and
canonicalization define their own operations and failure behavior when those
interfaces are introduced.

If a statement can be owned by a declaration, it does not belong here. This
document contains only the cross-contract derivation and architectural
consequences of Simulacra.
