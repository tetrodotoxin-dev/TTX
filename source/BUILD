# Copyright (c) 2023-present Matt Kaes and contributors

load("@tetro_toolchain//:defs.bzl", "library")
load("@tetro_toolchain//source/bazel:package.bzl", "source_files")

package(default_visibility = ["//visibility:public"])

source_files()

library(
    srcs = glob(["ttx/**/*.cpp"]),
    hdrs = glob([
        "ttx/**/*.h",
        "ttx/**/*.hpp",
    ]),
    includes = ["."],
    module = "ttx",
    deps = [
        "@perimortem//:core",
        "@perimortem//:memory",
        "@perimortem//:system",
    ],
)
