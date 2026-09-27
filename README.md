# TTX

TTX describes data formats and negotiates interfaces between independent
implementations. A consumer agrees on both what an interface means and how its
data and functions are represented before using it. The provider keeps its
private state and implementation behind that agreement.

The library has three layers:

- **[Data](source/data)** describes memory layouts, pointers and callable
  signatures. It compiles those descriptions into a canonical representation
  that participants can compare, and supplies protocols for borrowing data,
  filling destination storage or reading individual values.
- **[Semantic](source/semantic)** identifies contracts with UUIDs and binds them
  to concrete interfaces. It checks the requested API representation before
  exposing its operations. Flow establishes a data access agreement that later
  operations can reuse.
- **[Concept](source/concept)** builds on those interfaces to expose Abstracts,
  their relationships and the policies governing their answers. Providers can
  participate in the same graph while keeping different native object models.

Tetrodotoxin uses these contracts to share meaning between Dialects, compilers
and runtime modules. A Dialect defines its own semantics and exposes them through
Abstracts. Other parts of the toolchain ask those objects for the contracts they
need. An implementation can be replaced by another that fulfills the same
contracts, including one written in a different language.

The public boundary uses C records and function pointers, with C++ interfaces
over the same contracts. The implementation uses Perimortem for its native
storage and runtime support.

## Building and testing

On Linux x86_64, install Python 3 and the Bazel version specified in
[.bazelversion](.bazelversion). Bazel downloads the pinned LLVM tools, target SDK
and Perimortem dependency. The Linux target uses x86-64-v3 with RDRAND.

From the repository root, build the shared library and run the tests:

```sh
bazel build //:build
bazel run //tests:test
```

For an optimized build, tests and benchmarks:

```sh
bazel build --config=release //:build
bazel run --config=release //tests:test
bazel run --config=release //benchmarks:run
```

Build outputs are available under `.bin/bin/`.
