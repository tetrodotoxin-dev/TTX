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
    components = {
        "data": [],
        "semantic": ["data"],
        "graph": [
            "data",
            "semantic",
        ],
    },
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
    # Web runs as one static module. Native provider loading has no equivalent
    # in that runtime and its guarded tests contribute no Web dependencies.
    web_test_deps = [],
    web_threads = False,
)

vscode()
