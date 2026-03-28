import subprocess
Import("env")

try:
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    commit = "unknown"

env.Append(CPPDEFINES=[("GIT_COMMIT", f'\\"{commit}\\"')])