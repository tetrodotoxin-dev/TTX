# TTX

TTX is a library for composing independently developed systems regardless
of their backing runtime.

Instead of trying to unify every convention in a common runtime model, TTX
defines canonical data forms and semantically composable interfaces built
from policies and capabilities. These reusable building blocks allow systems
to negotiate interoperability without exposing either system's runtime
internals.

## Composable interfaces

TTX interfaces are composed of two primary parts: policies and capabilities.
Policies are used to negotiate how data can be used, while capabilities
expose operations that can be performed on that data under those policies.

These are not mutually exclusive systems. A TTX abstract interface can be
composed of any number of policies and capabilities. A policy can expose
capabilities of its own, and a capability can carry policies governing its
use. Each system can negotiate the minimum surface it needs when integrating
with other systems.

These categories are open to extension. Each system can define the contracts
its domain needs and compose them through the same negotiation model. If a
policy or capability can be used safely on its own, make it independently
negotiable. A combined interface should also support decomposition into the
minimum safe interface its provider is willing to expose. That smaller view
must preserve the policies required to use it.

## Semantic graph

TTX represents these abstract interfaces and their relationships in a
semantic graph. Each node is an abstract interface that can answer questions
by exposing related interfaces. A consumer discovers the graph through those
questions, following the relationships it needs and negotiating the
interfaces it encounters.

Each provider controls the relationships it exposes, which may lead to
interfaces supplied by other systems. Independently owned models can
therefore participate in the same semantic graph while keeping their own
implementations. Questions continue through the policies of the interfaces
encountered along the way, so the graph preserves the conditions under which
access is provided.

## Layers

TTX has three core layers building up from raw data streams to the full TTX
semantic graph:

- [Data](source/data) defines canonical forms for data and callable
  representations, with protocols for accessing and transferring data.
- [Semantics](source/semantic) negotiates contracts and their representations,
  connecting the agreed behavior to interfaces that can perform the work.
- [Concepts](source/concept) exposes full abstract interfaces and their
  composable relationships as a queryable graph. Each interface can offer
  capabilities and policies through further negotiation.

## Building and testing

Python 3 and Bazel are required. The Bazel version is specified in
[.bazelversion](.bazelversion).

```sh
bazel build //:build
bazel run //tests:test

bazel build --config=release //:build
bazel run --config=release //tests:test
bazel run --config=release //benchmarks:run
```
