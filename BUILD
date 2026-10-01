# Copyright (c) 2023-present Matt Kaes and contributors
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//:package.bzl", "package_release")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "headers",
    hdrs = glob([
        "source/**/*.h",
        "source/**/*.hpp",
    ]),
    includes = ["source"],
    deps = ["@perimortem"],
)

cc_library(
    name = "implementation",
    srcs = glob(["source/**/*.cpp"]),
    linkstatic = True,
    visibility = ["//:__subpackages__"],
    deps = [":headers"],
)

alias(
    name = "ttx",
    actual = ":implementation",
)

alias(
    name = "build",
    actual = ":implementation",
)

package_release(
    name = "sdk",
    target = ":implementation",
)
