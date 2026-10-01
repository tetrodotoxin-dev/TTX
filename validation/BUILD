# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//source/bazel:library.bzl", "LINUX")
load("@tetro_toolchain//source/bazel:validation.bzl", "benchmark", "test")

package(
    default_testonly = True,
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "sources",
    testonly = False,
    srcs = glob(["**"]),
)

test(
    name = "sdk_components",
    srcs = ["components.cpp"],
    deps = ["//:implementation"],
)

# Foreign providers stay in separate libraries so the suite exercises loading,
# borrowed code lifetime and independent C declarations. Building them in this
# package places them beside the runner for its executable-relative lookup.
cc_library(
    name = "provider_contract",
    hdrs = glob(["unit_tests/semantic/fixtures/*.h"]),
    deps = [
        "//:implementation",
    ],
)

cc_binary(
    name = "libflow_provider.so",
    srcs = ["unit_tests/semantic/fixtures/provider.c"],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "libheterogeneous_provider.so",
    srcs = ["unit_tests/semantic/fixtures/heterogeneous_provider.c"],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "libinterface_provider.so",
    srcs = ["unit_tests/semantic/fixtures/interface_provider.c"],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "libinvocation_provider.so",
    srcs = ["unit_tests/semantic/fixtures/invocation_provider.c"],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "libabstract_subject.so",
    srcs = [
        "unit_tests/concept/fixtures/subject.cpp",
        "unit_tests/concept/fixtures/subject.hpp",
    ],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = ["//:implementation"],
)

cc_binary(
    name = "libcomposition.so",
    srcs = [
        "unit_tests/concept/fixtures/composition.c",
        "unit_tests/concept/fixtures/composition.h",
    ],
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = ["//:implementation"],
)

test(
    name = "unit_tests",
    size = "small",
    srcs = glob(
        [
            "unit_tests/**/*.c",
            "unit_tests/**/*.cpp",
            "unit_tests/**/*.h",
            "unit_tests/**/*.hpp",
        ],
        # These providers are loaded from the independent libraries above.
        exclude = [
            "unit_tests/concept/fixtures/subject.cpp",
            "unit_tests/concept/fixtures/composition.c",
            "unit_tests/semantic/fixtures/heterogeneous_provider.c",
            "unit_tests/semantic/fixtures/interface_provider.c",
            "unit_tests/semantic/fixtures/invocation_provider.c",
            "unit_tests/semantic/fixtures/provider.c",
        ],
    ),
    data = [
        ":libabstract_subject.so",
        ":libcomposition.so",
        ":libflow_provider.so",
        ":libheterogeneous_provider.so",
        ":libinterface_provider.so",
        ":libinvocation_provider.so",
    ],
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
    deps = [
        "//:implementation",
    ],
)

benchmark(
    name = "benchmarks",
    srcs = glob(["benchmarks/**/*.cpp"]),
    deps = ["//:implementation"],
)
