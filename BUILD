# Copyright (c) 2023-present Matt Kaes and contributors
load("@tetro_toolchain//:library.bzl", "shared_library")

package(default_visibility = ["//visibility:public"])

# All three layers share one runtime. Independently loaded providers borrow
# Perimortem's process state through the same imported foundation.
shared_library(
    name = "ttx",
    deps = ["@perimortem"],
)
