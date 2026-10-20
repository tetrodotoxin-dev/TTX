# Copyright (c) 2023-present Matt Kaes and contributors

load("@tetro_toolchain//:defs.bzl", "validation")

# The independent workspace reuses the reviewed contract tests and providers.
# Their only TTX dependency is the candidate SDK imported from local archives.
alias(
    name = "ttx",
    actual = "@ttx//:ttx",
    visibility = ["//visibility:public"],
)

alias(
    name = "data",
    actual = "@ttx//:data",
    visibility = ["//visibility:public"],
)

alias(
    name = "semantic",
    actual = "@ttx//:semantic",
    visibility = ["//visibility:public"],
)

alias(
    name = "graph",
    actual = "@ttx//:graph",
    visibility = ["//visibility:public"],
)

validation(
    test_data = ["//validation/providers:libraries"],
    test_deps = [
        "//validation/providers:contracts",
        "//validation/support:library",
    ],
    deps = [":ttx"],
)
