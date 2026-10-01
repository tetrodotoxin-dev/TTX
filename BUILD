# Copyright (c) 2023-present Matt Kaes and contributors
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//source/bazel:library.bzl", "static_library")
load("@tetro_toolchain//source/bazel:package.bzl", "package_release")
load("@tetro_toolchain//source/bazel:vscode.bzl", "vscode")

package(default_visibility = ["//visibility:public"])

vscode(name = "vscode")

cc_library(
    name = "headers",
    hdrs = glob([
        "source/**/*.h",
        "source/**/*.hpp",
    ]),
    includes = ["source"],
    deps = [
        "@perimortem//:core",
        "@perimortem//:memory",
        "@perimortem//:system",
    ],
)

static_library(
    name = "implementation",
    srcs = glob(["source/**/*.cpp"]),
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
    static = ":implementation",
)
