use_repo(sdks, perimortem = "{perimortem}")

sdks.release(
    name = "ttx",
    archives = {archives},
    linkage = "{linkage}",
    project = "TTX",
    version = "{version}",
    deps = [
        "@perimortem//:core",
        "@perimortem//:memory",
    ],
)
sdks.component(
    name = "data",
    release = "ttx",
    deps = [
        "@perimortem//:core",
        "@perimortem//:memory",
    ],
)
sdks.component(
    name = "semantic",
    release = "ttx",
    deps = ["@perimortem//:core"],
)
use_repo(sdks, "ttx")
