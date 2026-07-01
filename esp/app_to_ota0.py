#  PlatformIO POST-script: point the MAIN app's flash offset at ota_0.
#
#  `board_build.app_partition_name = ota_0` (platformio.ini) already sizes the build
#  against ota_0. This sets the *flash* offset to ota_0 too — otherwise
#  `pio run -t upload` writes the ~955 KB app starting at the factory offset and overruns
#  the 640 KB factory slot. (Only runs for the unified factory+ota layout; a plain
#  two-slot table flashes ota_0 by default and is untouched.)
#
#  Booting ota_0 still needs otadata to select it — a bare upload leaves the bootloader
#  on factory. scripts/flash-main-app.sh does esptool (-> ota_0) + otatool (otadata).
import os

Import("env")  # noqa: F821

part_csv = os.path.join(
    env["PROJECT_DIR"],  # noqa: F821
    env.GetProjectOption("board_build.partitions", "partitions.csv"),  # noqa: F821
)

ota0_off = None
has_factory = False
try:
    with open(part_csv) as f:
        for line in f:
            cols = [c.strip() for c in line.split("#", 1)[0].split(",")]
            if len(cols) >= 5 and cols[2] == "ota_0":
                ota0_off = cols[3]
            if len(cols) >= 3 and cols[2] == "factory":
                has_factory = True
except OSError:
    pass

if ota0_off and has_factory:
    env.Replace(ESP32_APP_OFFSET=ota0_off)  # noqa: F821
    print(">> main app flash offset -> ota_0 @ %s (boot needs otatool; see flash-main-app.sh)"
          % ota0_off)
