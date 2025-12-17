from __future__ import annotations

import subprocess

from SCons.Script import Import  # type: ignore


def _git_describe(project_dir: str) -> str:
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty", "--always"],
            cwd=project_dir,
        )
        return out.decode().strip()
    except Exception:
        return "nogit"


Import("env")
version = _git_describe(env.subst("$PROJECT_DIR"))
escaped = f'\\"{version}\\"'
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", escaped)])
