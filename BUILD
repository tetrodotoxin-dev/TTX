# Copyright (c) 2023-present Matt Kaes and contributors

load("@tetro_toolchain//:defs.bzl", "benchmarks", "package", "tests", "vscode")

package(module = "ttx")

vscode()

tests(
    srcs = ["//validation:test_sources"],
    data = ["//validation/providers:libraries"],
    deps = [
        ":ttx",
        "//validation/providers:contracts",
        "//validation/support:library",
        "//validation/support:measurement",
    ],
)

benchmarks(
    srcs = ["//validation:benchmark_sources"],
    deps = [":ttx"],
)
