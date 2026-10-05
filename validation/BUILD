# Copyright (c) 2023-present Matt Kaes and contributors

load("@tetro_toolchain//source/bazel:package.bzl", "source_files")

package(default_visibility = ["//:__pkg__"])

# The source SDK includes this package and its provider and support packages.
source_files()

filegroup(
    name = "test_sources",
    testonly = True,
    srcs = glob([
        "unit_tests/**/*.c",
        "unit_tests/**/*.cpp",
        "unit_tests/**/*.h",
        "unit_tests/**/*.hpp",
    ]),
)

filegroup(
    name = "benchmark_sources",
    testonly = True,
    srcs = glob(["benchmarks/**/*.cpp"]),
)
