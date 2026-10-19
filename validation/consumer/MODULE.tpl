sdks.release(
    name = "ttx",
    archives = {archives},
    deps = ["@perimortem//:core", "@perimortem//:memory", "@perimortem//:serialization"],
    linkage = "{linkage}",
    project = "TTX",
    version = "{version}",
)
use_repo(sdks, "ttx")
