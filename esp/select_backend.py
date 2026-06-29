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
