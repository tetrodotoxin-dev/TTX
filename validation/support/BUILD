# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//:defs.bzl", "LINUX")
load("@tetro_toolchain//source/bazel:package.bzl", "source_files")

package(default_visibility = ["//:__pkg__"])

source_files()

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

cc_library(
    name = "measurement",
    testonly = True,
    srcs = ["measurement.cpp"],
    hdrs = ["measurement.hpp"],
    linkopts = select({
        LINUX: [
            # ThinLTO can introduce memmove after discarding an unreferenced wrapper.
            # Retain the instrumentation entry until the linker rewrites those calls.
            "-Wl,--undefined=__wrap_memmove",
            "-Wl,--wrap=memmove",
            "-Wl,--wrap=__asan_memmove",
            "-Wl,--wrap=malloc",
            "-Wl,--wrap=calloc",
            "-Wl,--wrap=realloc",
            "-Wl,--wrap=aligned_alloc",
            "-Wl,--wrap=posix_memalign",
            "-Wl,--wrap=_Znwm",
            "-Wl,--wrap=_Znam",
            "-Wl,--wrap=_ZnwmSt11align_val_t",
            "-Wl,--wrap=_ZnamSt11align_val_t",
        ],
        "//conditions:default": [],
    }),
    deps = ["@perimortem//:core"],
)
