# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//:__pkg__"])

cc_library(
    name = "library",
    testonly = True,
    srcs = ["library.cpp"],
    hdrs = ["library.hpp"],
    deps = [
        "@perimortem//:core",
        "@perimortem//:memory",
        "@perimortem//:system",
    ],
)
