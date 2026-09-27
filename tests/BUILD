# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//:validation.bzl", "test")

package(default_testonly = True, default_visibility = ["//visibility:public"])

filegroup(name = "sources", testonly = False, srcs = glob(["**"]))

# Foreign providers stay in separate libraries so the suite exercises loading,
# borrowed code lifetime and independent C declarations. Building them in this
# package places them beside the runner for its executable-relative lookup.
cc_library(
    name = "provider_contract",
    hdrs = glob(["semantic/fixtures/*.h"]),
    deps = [
        "//:implementation",
    ],
)

cc_binary(
    name = "libflow_provider.so",
    srcs = ["semantic/fixtures/provider.c"],
    copts = [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic-errors",
    ],
    linkshared = True,
    deps = [":provider_contract"],
)

cc_binary(
    name = "libheterogeneous_provider.so",
    srcs = ["semantic/fixtures/heterogeneous_provider.c"],
    copts = [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic-errors",
    ],
    linkshared = True,
    deps = [":provider_contract"],
)

cc_binary(
    name = "libinterface_provider.so",
    srcs = ["semantic/fixtures/interface_provider.c"],
    copts = [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic-errors",
    ],
    linkshared = True,
    deps = [":provider_contract"],
)

cc_binary(
    name = "libinvocation_provider.so",
    srcs = ["semantic/fixtures/invocation_provider.c"],
    copts = [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic-errors",
    ],
    linkshared = True,
    deps = [":provider_contract"],
)

cc_binary(
    name = "libabstract_subject.so",
    srcs = [
        "concept/fixtures/subject.cpp",
        "concept/fixtures/subject.hpp",
    ],
    linkshared = True,
    deps = ["//:implementation"],
)

test(
    name = "test",
    size = "small",
    srcs = glob(
        [
            "**/*.c",
            "**/*.cpp",
            "**/*.h",
            "**/*.hpp",
        ],
        # These owners are loaded from the independent libraries above.
        exclude = [
            "concept/fixtures/subject.cpp",
            "semantic/fixtures/heterogeneous_provider.c",
            "semantic/fixtures/interface_provider.c",
            "semantic/fixtures/invocation_provider.c",
            "semantic/fixtures/provider.c",
        ],
    ),
    data = [
        ":libabstract_subject.so",
        ":libflow_provider.so",
        ":libheterogeneous_provider.so",
        ":libinterface_provider.so",
        ":libinvocation_provider.so",
    ],
    linkopts = [
        "-ldl",
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
    deps = [
        "//:implementation",
    ],
)
