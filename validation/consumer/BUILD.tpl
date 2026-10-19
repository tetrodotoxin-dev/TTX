# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@tetro_toolchain//:defs.bzl", "validation")

cc_binary(
    name = "c_consumer",
    srcs = ["consumer.c"],
    deps = [
        "@perimortem",
        "@ttx",
    ],
)

cc_binary(
    name = "cpp_consumer",
    srcs = ["consumer.cpp"],
    deps = [
        "@perimortem",
        "@ttx",
    ],
)

# The independent workspace reuses the reviewed contract tests and providers.
# Their only TTX dependency is the candidate SDK imported from local archives.
alias(
    name = "ttx",
    actual = "@ttx//:ttx",
    visibility = ["//visibility:public"],
)

validation(
    deps = [":ttx"],
    test_data = ["//validation/providers:libraries"],
    test_deps = [
        "//validation/providers:contracts",
        "//validation/support:library",
    ],
)
