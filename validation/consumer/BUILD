# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "c_consumer",
    srcs = ["consumer.c"],
    deps = ["//:data"],
)

cc_binary(
    name = "data_consumer",
    srcs = ["data.cpp"],
    deps = ["//:data"],
)

cc_binary(
    name = "semantic_consumer",
    srcs = ["semantic.cpp"],
    deps = ["//:semantic"],
)

cc_binary(
    name = "cpp_consumer",
    srcs = ["consumer.cpp"],
    deps = ["//:concept"],
)
