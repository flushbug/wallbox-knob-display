# PlatformIO post-build script: after every build, write flashable images to bin/
#   <name>-<version>-full.bin  bootloader + partitions + app, flash at 0x0 (first install)
#   <name>-<version>-ota.bin   app only, for the web update / OTA
# <name> comes from custom_image_name in platformio.ini (one per board).
Import("env")  # noqa: F821
import os
import re
import shutil


def _version(env):
    for flag in env.get("CPPDEFINES", []):
        if isinstance(flag, (list, tuple)) and flag[0] == "FW_VERSION":
            return re.sub(r'[\\"]', "", str(flag[1]))
    return "dev"


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    app = os.path.join(build_dir, env.subst("${PROGNAME}.bin"))
    out_dir = os.path.join(env.subst("$PROJECT_DIR"), "bin")
    os.makedirs(out_dir, exist_ok=True)
    name = env.GetProjectOption("custom_image_name", env.subst("$PIOENV"))
    for f in os.listdir(out_dir):  # keep only the current version of this board
        if f.startswith(name + "-") and f.endswith(".bin"):
            os.remove(os.path.join(out_dir, f))

    base = os.path.join(out_dir, name + "-" + _version(env))
    full = base + "-full.bin"
    ota = base + "-ota.bin"

    images = []
    for offset, path in env.get("FLASH_EXTRA_IMAGES", []):
        images += [env.subst(offset), env.subst(path)]
    images += [env.subst("$ESP32_APP_OFFSET"), app]

    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    cmd = [env.subst("$PYTHONEXE"), esptool, "--chip", env.BoardConfig().get("build.mcu"),
           "merge_bin", "-o", full,
           "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep"] + images
    if env.Execute(" ".join('"%s"' % c for c in cmd)):
        raise SystemExit("merge_bin failed")
    shutil.copyfile(app, ota)
    print("Flashable image: %s (flash at 0x0)" % full)
    print("OTA image:       %s" % ota)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
