# Copyright (c) 2023-present Matt Kaes and contributors

load("@tetro_toolchain//:defs.bzl", "WINDOWS", "library")

package(default_visibility = ["//visibility:public"])

_COMPONENTS = {
    "data": [],
    "semantic": ["data"],
    "concept": [
        "data",
        "semantic",
    ],
}

_COMPONENT_DEPS = {
    "data": [
        "@perimortem//:core",
        "@perimortem//:memory",
    ],
    "semantic": ["@perimortem//:serialization"],
}

_LIBRARY = dict(
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
        "@perimortem//:serialization",
    ],
)

library(
    name = "ttx_static",
    srcs = _LIBRARY["srcs"],
    hdrs = _LIBRARY["hdrs"],
    component_deps = _COMPONENT_DEPS,
    components = _COMPONENTS,
    includes = _LIBRARY["includes"],
    module = "ttx",
    deps = _LIBRARY["deps"],
)

library(
    name = "ttx_shared",
    srcs = _LIBRARY["srcs"],
    hdrs = _LIBRARY["hdrs"],
    includes = _LIBRARY["includes"],
    linkage = "shared",
    module = "ttx",
    shared_lib_name = select({
        WINDOWS: "ttx.dll",
        "//conditions:default": "libttx.so",
    }),
    deps = _LIBRARY["deps"],
)
