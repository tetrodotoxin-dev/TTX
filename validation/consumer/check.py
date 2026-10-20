# Copyright (c) 2023-present Matt Kaes and contributors

"""Build the candidate SDK, then consume its static and shared Linux archives.

Run from the repository with `python3 validation/consumer/check.py`. The normal
release importer receives the candidate checksums and finds those exact files
through Bazel's distdir. Toolchain and Perimortem keep their published pins.
The temporary consumer contains only validation sources and build declarations;
all TTX headers and libraries come from the built SDK.
"""

import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def check():
    repository = Path(__file__).resolve().parents[2]
    templates = repository / "validation/consumer"
    subprocess.run(["bazel", "build", ":sdk", "--config=release"],
                   cwd=repository, check=True)
    directory = Path(subprocess.check_output(
        ["bazel", "info", "bazel-bin", "--config=release"],
        cwd=repository, text=True).strip())
    version = re.search(r'version\s*=\s*"([^"]+)"',
                        (repository / "MODULE.bazel").read_text()).group(1)
    prefix = "ttx-" + version + "-"
    pins = {}
    for entry in (directory / (prefix + "sha256.txt")).read_text().splitlines():
        digest, name = entry.split()
        key = name.removeprefix(prefix).removesuffix(".zip")
        if key == "headers" or key.startswith("linux-x86_64-v3-"):
            if hashlib.sha256((directory / name).read_bytes()).hexdigest() != digest:
                raise ValueError("SDK checksum mismatch: " + name)
            pins[key] = digest

    with tempfile.TemporaryDirectory(prefix="ttx-sdk-consumer-") as temporary:
        workspace = Path(temporary) / "consumer"
        workspace.mkdir()
        for name in [".bazelversion", ".bazelrc"]:
            shutil.copyfile(repository / name, workspace / name)
        shutil.copytree(templates, workspace / "validation/consumer")
        shutil.copyfile(templates / "BUILD.tpl", workspace / "BUILD")

        # Use the same adversarial providers and assertions in the source build
        # and the independent SDK build. Only their dependency boundary changes.
        for name in ["providers", "support", "tests", "benchmarks"]:
            shutil.copytree(repository / "validation" / name,
                            workspace / "validation" / name)

        # Carry the source build's published dependencies into the consumer.
        # Only the TTX dependency comes from the candidate archives.
        dependencies = re.sub(
            r"\Amodule\([\s\S]*?\)", 'module(name="ttx_consumer")',
            (repository / "MODULE.bazel").read_text(), count=1)

        for linkage in ["static", "shared"]:
            module = dependencies + "\n" + (templates / "MODULE.tpl").read_text()
            for key, value in {"archives": repr(pins), "linkage": linkage,
                               "version": version}.items():
                module = module.replace("{" + key + "}", value)
            (workspace / "MODULE.bazel").write_text(module)
            for target in [
                "//validation/consumer:c_consumer",
                "//validation/consumer:data_consumer",
                "//validation/consumer:semantic_consumer",
                "//validation/consumer:cpp_consumer",
                "//:tests",
            ]:
                subprocess.run([
                    "bazel", "--batch", "--output_base=" + str(Path(temporary) / "bazel"),
                    "run", target, "--config=release",
                    "--distdir=" + str(directory),
                ], cwd=workspace, check=True)
            print(
                linkage + " candidate SDK: Data, Semantic, Concept consumers "
                "and contract tests passed.", flush=True)


if __name__ == "__main__":
    check()
