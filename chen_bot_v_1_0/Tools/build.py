"""Build chen_bot_v_1_0: host unit tests with GCC, firmware with Arm Compiler 5.

Usage:
    python Tools/build.py host
    python Tools/build.py arm --profile all
"""
import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path

DEV = Path(__file__).resolve().parents[1]
ROOT = DEV.parent              # rm_v_1_0
FW = DEV / "Firmware"
VENDOR = ROOT / "hard1/hard_lib_1/MC02_data/mc02_control"
BUILD = ROOT / "build/chen_bot_v_1_0"

PROFILES = {"SIM": 1, "BENCH": 2, "RUN": 3}

# Modules with no HAL/FreeRTOS dependency: compiled for the host test binary.
PURE = [
    FW / "Config/params.c",
    FW / "Control/chassis.c",
    FW / "Control/pid.c",
    FW / "Control/safety.c",
    FW / "Control/servo_ctrl.c",
    FW / "Debug/debug_service.c",
    FW / "Device/keys.c",
    FW / "Device/Mock/mock.c",
    FW / "Service/faults.c",
    FW / "Service/protocol.c",
    FW / "Service/snapshot.c",
]

HOST_INCLUDES = ["Config", "Device", "Device/Mock", "Control", "Service", "Debug"]

ARM_INCLUDES = ["Config", "BSP", "Device", "Device/Real", "Device/Mock",
                "Control", "Service", "Debug", "APP", "HMI"]

ARM_FILES = [
    "main.c",
    "APP/app.c",
    "BSP/board.c",
    "BSP/lcd.c",
    "BSP/uart_link.c",
    "BSP/timebase.c",
    "BSP/can_link.c",
    "Config/params.c",
    "Control/chassis.c",
    "Control/pid.c",
    "Control/safety.c",
    "Control/servo_ctrl.c",
    "Debug/debug_service.c",
    "Device/keys.c",
    "Device/Mock/mock.c",
    "Device/Real/real_devices.c",
    "HMI/hmi.c",
    "Service/faults.c",
    "Service/protocol.c",
    "Service/snapshot.c",
]

HAL_MODULES = ("", "_cortex", "_rcc", "_rcc_ex", "_flash", "_flash_ex", "_pwr", "_pwr_ex",
               "_gpio", "_tim", "_tim_ex", "_uart", "_uart_ex", "_dma", "_dma_ex",
               "_adc", "_adc_ex", "_spi", "_spi_ex", "_fdcan")


def run(argv):
    subprocess.run([str(x) for x in argv], check=True)


def host(out):
    out.mkdir(parents=True, exist_ok=True)
    exe = out / "tests.exe"
    cc = os.getenv("CHEN_HOST_CC", "gcc")
    run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2",
         "-DCHEN_PROFILE=2", "-DCHEN_FEATURE_ACTUATION=1",
         *("-I" + str(FW / p) for p in HOST_INCLUDES),
         *PURE, FW.parent / "Tests" / "test_core.c", "-o", exe])
    run([exe])
    print(f"host tests ok: {exe}")


def patch_conf(dest: Path):
    """Enable the HAL modules this firmware needs, in a build-local copy."""
    text = (VENDOR / "Core/Inc/stm32h7xx_hal_conf.h").read_text()
    needed = ["HAL_UART_MODULE_ENABLED", "HAL_ADC_MODULE_ENABLED", "HAL_SPI_MODULE_ENABLED",
              "HAL_TIM_MODULE_ENABLED", "HAL_DMA_MODULE_ENABLED", "HAL_GPIO_MODULE_ENABLED"]
    for module in needed:
        if not re.search(rf"^\s*#define\s+{module}\b", text, re.M):
            text = text.replace("#define HAL_MODULE_ENABLED",
                                f"#define HAL_MODULE_ENABLED\n#define {module}", 1)
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "stm32h7xx_hal_conf.h").write_text(text)


def arm(profile, out, arm_bin):
    hal = VENDOR / "Drivers/STM32H7xx_HAL_Driver"
    cmsis = VENDOR / "Drivers/CMSIS"
    rtos = VENDOR / "Middlewares/Third_Party/FreeRTOS/Source"
    port = rtos / "portable/RVDS/ARM_CM4F"

    conf = out / "config"
    patch_conf(conf)

    includes = [conf, *(FW / p for p in ARM_INCLUDES),
                hal / "Inc", cmsis / "Include",
                cmsis / "Device/ST/STM32H7xx/Include", rtos / "include", port]

    files = [FW / name for name in ARM_FILES]
    files.append(VENDOR / "Core/Src/system_stm32h7xx.c")
    files.extend(hal / f"Src/stm32h7xx_hal{name}.c" for name in HAL_MODULES)
    files.extend([rtos / "tasks.c", rtos / "list.c", rtos / "queue.c", port / "port.c"])

    profile_id = PROFILES[profile]
    cpu = "Cortex-M7.fp.dp"
    startup = out / "startup.o"
    run([arm_bin / "armasm.exe", "--cpu", cpu,
         VENDOR / "MDK-ARM/startup_stm32h723xx.s", "-o", startup])

    objects = [startup]
    for path in files:
        obj = out / (path.stem + ".o")
        run([arm_bin / "armcc.exe", "--cpu", cpu, "--c99", "-O2", "--split_sections",
             "-DSTM32H723xx", "-DUSE_HAL_DRIVER", "-DHSE_VALUE=24000000UL",
             f"-DCHEN_PROFILE={profile_id}",
             *("-I" + str(p) for p in includes), "-c", path, "-o", obj])
        objects.append(obj)

    elf = out / "chen_bot_v_1_0.axf"
    run([arm_bin / "armlink.exe", "--scatter", FW / "BSP/layout.sct", "--map",
         "--list", out / "chen_bot_v_1_0.map", "--info", "sizes,totals",
         "-o", elf, *objects])
    print(f"{profile}: {elf}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=["host", "arm"])
    parser.add_argument("--profile", choices=[*PROFILES, "all"], default="all")
    default_arm = os.getenv("CHEN_ARMCC_BIN",
                            str(Path(shutil.which("armcc") or
                                     "D:/keil5/ARM/ARMCC/bin/armcc.exe").parent))
    parser.add_argument("--arm-bin", type=Path, default=default_arm)
    args = parser.parse_args()

    if args.target == "host":
        host(BUILD / "host")
        return

    for profile in (PROFILES if args.profile == "all" else [args.profile]):
        arm(profile, BUILD / "arm" / profile, args.arm_bin)


if __name__ == "__main__":
    main()
