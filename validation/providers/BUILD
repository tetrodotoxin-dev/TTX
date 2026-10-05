# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@tetro_toolchain//source/bazel:package.bzl", "source_files")

source_files()

# The runner compiles against the provider contracts and loads their binaries
# through the entry points exercised by the tests.
cc_library(
    name = "contracts",
    testonly = True,
    hdrs = glob(["*/*.h"]),
    visibility = ["//:__pkg__"],
    deps = ["//:ttx"],
)

# Each provider using this helper keeps its compiled forms in its own module.
cc_library(
    name = "representation",
    testonly = True,
    hdrs = ["representation.h"],
    deps = ["//:ttx"],
)

cc_binary(
    name = "libabstract_subject.so",
    testonly = True,
    srcs = glob(["abstract/*.cpp"]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = ["//:ttx"],
)

cc_binary(
    name = "libcomposition.so",
    testonly = True,
    srcs = glob([
        "composition/*.c",
        "composition/*.h",
    ]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = ["//:ttx"],
)

cc_binary(
    name = "libflow_provider.so",
    testonly = True,
    srcs = glob([
        "flow/*.c",
        "flow/*.h",
    ]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":representation"],
)

cc_binary(
    name = "libheterogeneous_provider.so",
    testonly = True,
    srcs = glob([
        "heterogeneous/*.c",
        "heterogeneous/*.h",
    ]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":representation"],
)

cc_binary(
    name = "libinterface_provider.so",
    testonly = True,
    srcs = glob([
        "interface/*.c",
        "interface/*.h",
    ]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = [":representation"],
)

cc_binary(
    name = "libinvocation_provider.so",
    testonly = True,
    srcs = glob([
        "invocation/*.c",
        "invocation/*.h",
    ]),
    linkshared = True,
    local_defines = ["TTX_TEST_EXPORT=1"],
    deps = ["//:ttx"],
)

filegroup(
    name = "libraries",
    testonly = True,
    srcs = [
        ":libabstract_subject.so",
        ":libcomposition.so",
        ":libflow_provider.so",
        ":libheterogeneous_provider.so",
        ":libinterface_provider.so",
        ":libinvocation_provider.so",
    ],
    visibility = ["//:__pkg__"],
)
