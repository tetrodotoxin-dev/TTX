sdks.release(
    name = "ttx",
    archives = {archives},
    deps = ["@perimortem//:core", "@perimortem//:memory", "@perimortem//:serialization"],
    linkage = "{linkage}",
    project = "TTX",
    version = "{version}",
)
sdks.component(
    release = "ttx",
    name = "data",
    deps = ["@perimortem//:core", "@perimortem//:memory"],
)
sdks.component(
    release = "ttx",
    name = "semantic",
    deps = ["@perimortem//:serialization"],
)
use_repo(sdks, "ttx")
