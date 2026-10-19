# Copyright (c) 2023-present Matt Kaes and contributors

load(
    "@tetro_toolchain//:defs.bzl",
    "format",
    "package",
    "snapshot",
    "validation",
    "vscode",
)

package(
    linkage = "shared",
    module = "ttx",
    shared = "//source:ttx_shared",
    static = "//source:ttx_static",
)

format()

snapshot()

validation(
    test_data = ["//validation/providers:libraries"],
    test_deps = [
        "//validation/providers:contracts",
        "//validation/support:library",
    ],
)

vscode()
