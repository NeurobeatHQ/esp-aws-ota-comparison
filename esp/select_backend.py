#  PlatformIO pre-script: export the chosen OTA backend as an environment
#  variable so CMake (root + the IDF requirements script pass) can read it via
#  $ENV{OTA_BACKEND}. The backend == the build-env name: mqtt | https | jobs | manual.
import os
import sys

Import("env")  # noqa: F821 (provided by PlatformIO)

VALID = ("mqtt", "https", "jobs", "manual")
backend = env["PIOENV"]  # noqa: F821
if backend not in VALID:
    sys.stderr.write(
        "error: env '%s' is not an OTA backend; use one of %s\n" % (backend, ", ".join(VALID))
    )
    env.Exit(1)  # noqa: F821

os.environ["OTA_BACKEND"] = backend
print(">> OTA backend = %s" % backend)

# macOS Finder drops .DS_Store files into managed_components/, which the ESP-IDF
# component manager rejects as "unexpected files" / "component modified on disk" and
# HARD-FAILS the build. This pre-script runs before CMake's component pass, so sweep
# them here; also ask the manager to ignore any that reappear mid-build.
_mc = os.path.join(env.subst("$PROJECT_DIR"), "managed_components")  # noqa: F821
for _root, _dirs, _files in os.walk(_mc):
    if ".DS_Store" in _files:
        try:
            os.remove(os.path.join(_root, ".DS_Store"))
        except OSError:
            pass
os.environ["IGNORE_UNKNOWN_FILES_FOR_MANAGED_COMPONENTS"] = "1"

