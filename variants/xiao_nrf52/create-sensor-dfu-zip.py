"""Keep a stable name for the sensor's PlatformIO-generated DFU package."""

from shutil import copyfile

Import("env")


def copy_dfu_zip(target, source, env):
    copyfile(source[0].get_abspath(), target[0].get_abspath())


named_zip = env.Command(
    "$BUILD_DIR/Xiao_nrf52_bme680_sensor.zip",
    "$BUILD_DIR/${PROGNAME}.zip",
    copy_dfu_zip,
)
env.Default(named_zip)
