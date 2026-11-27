"""Build the MC02 firmware with the local Arm Compiler 5 or run host C tests."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess

DEV = Path(__file__).resolve().parents[1]
ROOT = DEV.parents[1]
FW = DEV / "Firmware"
VENDOR = ROOT / "hard1/hard_lib_1/MC02_data/mc02_control"
COMMON = [FW / part for part in (
    "Control/safety.c", "Service/protocol.c", "Debug/service.c", "APP/keys.c"
)]
MOCK = FW / "Device/Mock/mock.c"
INCLUDES = [FW / part for part in ("Config", "Device", "Control", "Service", "Debug", "APP", "BSP", "Device/Mock", "HMI", "LcdSmoke")]
PROFILES = {"SIM": 1, "BENCH": 2, "RUN": 3}


def run(argv):
    subprocess.run([str(x) for x in argv], check=True)


def host(profile, out):
    exe = out / "tests.exe"
    run([os.getenv("RM_HOST_CC", "gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-pedantic", "-O2", f"-DRM_PROFILE={PROFILES[profile]}",
         *("-I" + str(p) for p in INCLUDES), *COMMON, FW / "Control/actuation.c", *([MOCK] if profile == "SIM" else []),
         FW / "Test/test_core.c", "-o", exe])
    run([exe, DEV / "Protocol/vectors.txt"])
    if profile == "SIM":
        run([os.getenv("RM_HOST_CC", "gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
             "-pedantic", "-O2", "-DRM_PROFILE=1", *("-I" + str(p) for p in INCLUDES),
             *COMMON, MOCK, FW / "Test/sim_link.c", "-o", out / "sim_link.exe"])


def host_hmi():
    out = ROOT / "build/host/HMI"
    out.mkdir(parents=True, exist_ok=True)
    exe = out / "test_hmi.exe"
    run([os.getenv("RM_HOST_CC", "gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-pedantic", "-O2", *("-I" + str(p) for p in INCLUDES),
         FW / "HMI/hmi.c", FW / "Test/test_hmi.c", "-o", exe])
    run([exe])


def arm(profile, out, arm_bin):
    hal = VENDOR / "Drivers/STM32H7xx_HAL_Driver"
    cmsis = VENDOR / "Drivers/CMSIS"
    rtos = VENDOR / "Middlewares/Third_Party/FreeRTOS/Source"
    port = rtos / "portable/RVDS/ARM_CM4F"
    # Patch only a build-local copy of the vendor HAL configuration.
    inc = out / "config"
    inc.mkdir(exist_ok=True)
    vendor_conf = (VENDOR / "Core/Inc/stm32h7xx_hal_conf.h").read_text()
    if not re.search(r"^\s*#define HAL_UART_MODULE_ENABLED\b", vendor_conf, re.M):
        vendor_conf = vendor_conf.replace("#define HAL_MODULE_ENABLED",
                                          "#define HAL_MODULE_ENABLED\n#define HAL_UART_MODULE_ENABLED")
    (inc / "stm32h7xx_hal_conf.h").write_text(vendor_conf)
    includes = [inc, *INCLUDES, hal / "Inc", cmsis / "Include",
                cmsis / "Device/ST/STM32H7xx/Include", rtos / "include", port]
    names = ("", "_cortex", "_rcc", "_rcc_ex", "_flash", "_flash_ex", "_pwr", "_pwr_ex",
             "_gpio", "_tim", "_tim_ex", "_uart", "_uart_ex", "_dma", "_dma_ex")
    files = [*COMMON, FW / "BSP/board.c", FW / "BSP/timebase.c", FW / "APP/main.c",
             VENDOR / "Core/Src/system_stm32h7xx.c",
             *(hal / f"Src/stm32h7xx_hal{name}.c" for name in names),
             rtos / "tasks.c", rtos / "list.c", rtos / "queue.c", port / "port.c"]
    if profile == "SIM":
        files.append(MOCK)
    else:
        files.extend((FW / "APP/lcd_runtime.c", FW / "LcdSmoke/panel.c", FW / "HMI/hmi.c"))
        files.extend(hal / f"Src/stm32h7xx_hal{name}.c"
                     for name in ("_adc", "_adc_ex", "_spi", "_spi_ex"))
    cpu = "Cortex-M7.fp.dp"
    startup = out / "startup.o"
    run([arm_bin / "armasm.exe", "--cpu", cpu, VENDOR / "MDK-ARM/startup_stm32h723xx.s", "-o", startup])
    objects = [startup]
    for path in files:
        obj = out / (path.stem + ".o")
        run([arm_bin / "armcc.exe", "--cpu", cpu, "--c99", "-O2", "--split_sections",
             "-DSTM32H723xx", "-DUSE_HAL_DRIVER", "-DHSE_VALUE=24000000UL", f"-DRM_PROFILE={PROFILES[profile]}",
             *("-I" + str(p) for p in includes), "-c", path, "-o", obj])
        objects.append(obj)
    elf = out / "rm_mc02.axf"
    run([arm_bin / "armlink.exe", "--scatter", FW / "BSP/layout.sct", "--map",
         "--list", out / "rm_mc02.map", "--info", "sizes,totals", "-o", elf, *objects])
    print(f"{profile}: {elf}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=["host", "arm"])
    parser.add_argument("--profile", choices=[*PROFILES, "all"], default="all")
    default_arm = os.getenv("RM_ARMCC_BIN", str(Path(shutil.which("armcc") or
                            "D:/keil5/ARM/ARMCC/bin/armcc.exe").parent))
    parser.add_argument("--arm-bin", type=Path, default=default_arm)
    args = parser.parse_args()
    for profile in (PROFILES if args.profile == "all" else [args.profile]):
        out = ROOT / "build" / args.target / profile
        out.mkdir(parents=True, exist_ok=True)
        if args.target == "host":
            host(profile, out)
        else:
            arm(profile, out, args.arm_bin)
    if args.target == "host":
        host_hmi()


if __name__ == "__main__":
    main()
