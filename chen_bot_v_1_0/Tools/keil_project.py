"""Generate MDK-ARM/chen_bot_v_1_0.uvprojx from the proven LCD_SMOKE template.

Group layout follows the reference project convention (RM2021_Hero):
Application/..., Drivers/..., Middlewares/... so the tree reads like a normal
Keil workspace instead of one group per file.
"""
import re
import xml.etree.ElementTree as ET
from pathlib import Path

DEV = Path(__file__).resolve().parents[1]
ROOT = DEV.parent
TEMPLATE = ROOT / "soft1/development/Firmware/MDK-ARM/MC02_LCD_Smoke.uvprojx"
OUT = DEV / "MDK-ARM/chen_bot_v_1_0.uvprojx"
VENDOR = ROOT / "hard1/hard_lib_1/MC02_data/mc02_control"

GROUPS = [
    ("Application/User/APP", ["Firmware/main.c", "Firmware/APP/app.c"], 1),
    ("Application/User/HMI", ["Firmware/HMI/hmi.c"], 1),
    ("Application/User/Control", [
        "Firmware/Control/chassis.c",
        "Firmware/Control/pid.c",
        "Firmware/Control/safety.c",
        "Firmware/Control/servo_ctrl.c",
    ], 1),
    ("Application/User/Service", [
        "Firmware/Service/faults.c",
        "Firmware/Service/protocol.c",
        "Firmware/Service/snapshot.c",
    ], 1),
    ("Application/User/Debug", ["Firmware/Debug/debug_service.c"], 1),
    ("Application/User/Config", ["Firmware/Config/params.c"], 1),
    ("Drivers/BSP", [
        "Firmware/BSP/board.c",
        "Firmware/BSP/lcd.c",
        "Firmware/BSP/uart_link.c",
        "Firmware/BSP/timebase.c",
    ], 1),
    ("Drivers/Devices", [
        "Firmware/Device/keys.c",
        "Firmware/Device/Real/real_devices.c",
    ], 1),
    ("Drivers/Devices/Mock", ["Firmware/Device/Mock/mock.c"], 1),
]

HAL = ["", "_cortex", "_rcc", "_rcc_ex", "_flash", "_flash_ex", "_pwr", "_pwr_ex",
       "_gpio", "_tim", "_tim_ex", "_uart", "_uart_ex", "_dma", "_dma_ex",
       "_adc", "_adc_ex", "_spi", "_spi_ex"]

INCLUDES = [
    "config",
    "..\\Firmware\\Config", "..\\Firmware\\BSP", "..\\Firmware\\Device",
    "..\\Firmware\\Device\\Real", "..\\Firmware\\Device\\Mock", "..\\Firmware\\Control",
    "..\\Firmware\\Service", "..\\Firmware\\Debug", "..\\Firmware\\APP", "..\\Firmware\\HMI",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Core\\Inc",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Drivers\\STM32H7xx_HAL_Driver\\Inc",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Drivers\\CMSIS\\Include",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Drivers\\CMSIS\\Device\\ST\\STM32H7xx\\Include",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Middlewares\\Third_Party\\FreeRTOS\\Source\\include",
    "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\Middlewares\\Third_Party\\FreeRTOS\\Source\\portable\\RVDS\\ARM_CM4F",
]

DEFINES = "STM32H723xx,USE_HAL_DRIVER,HSE_VALUE=24000000UL,CHEN_PROFILE=2,CHEN_FEATURE_ACTUATION=0"

MODULES = ["HAL_UART_MODULE_ENABLED", "HAL_ADC_MODULE_ENABLED", "HAL_SPI_MODULE_ENABLED",
           "HAL_TIM_MODULE_ENABLED", "HAL_DMA_MODULE_ENABLED", "HAL_GPIO_MODULE_ENABLED"]


def vendor(rel):
    return "..\\..\\hard1\\hard_lib_1\\MC02_data\\mc02_control\\" + rel.replace("/", "\\")


def local(rel):
    return "..\\" + rel.replace("/", "\\")


def write_hal_conf():
    """Vendor conf leaves UART/ADC disabled; uVision has no patch step."""
    source = (VENDOR / "Core/Inc/stm32h7xx_hal_conf.h").read_text()
    for module in MODULES:
        if not re.search(rf"^\s*#define\s+{module}\b", source, re.M):
            source = source.replace("#define HAL_MODULE_ENABLED",
                                    f"#define HAL_MODULE_ENABLED\n#define {module}", 1)
    target = DEV / "MDK-ARM/config/stm32h7xx_hal_conf.h"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(source)


def add_group(groups, name, entries):
    group = ET.SubElement(groups, "Group")
    ET.SubElement(group, "GroupName").text = name
    files = ET.SubElement(group, "Files")
    for path, file_type in entries:
        f = ET.SubElement(files, "File")
        ET.SubElement(f, "FileName").text = Path(path.replace("\\", "/")).name
        ET.SubElement(f, "FileType").text = str(file_type)
        ET.SubElement(f, "FilePath").text = path


def main():
    write_hal_conf()

    tree = ET.parse(TEMPLATE)
    root = tree.getroot()
    target = root.find("./Targets/Target")
    target.find("TargetName").text = "CHEN_BOT"
    tco = target.find("./TargetOption/TargetCommonOption")
    tco.find("OutputName").text = "chen_bot_v_1_0"
    tco.find("OutputDirectory").text = "..\\..\\build\\keil\\chen_bot_v_1_0\\"
    tco.find("ListingPath").text = "..\\..\\build\\keil\\chen_bot_v_1_0\\"

    ads = target.find("./TargetOption/TargetArmAds")
    vc = ads.find("./Cads/VariousControls")
    vc.find("Define").text = DEFINES
    vc.find("IncludePath").text = ";".join(INCLUDES)
    ads.find("./LDads/ScatterFile").text = "..\\Firmware\\BSP\\layout.sct"

    groups = target.find("Groups")
    for child in list(groups):
        groups.remove(child)

    for name, entries, file_type in GROUPS:
        add_group(groups, name, [(local(p), file_type) for p in entries])

    add_group(groups, "Application/MDK-ARM", [(vendor("MDK-ARM/startup_stm32h723xx.s"), 2)])
    add_group(groups, "Drivers/CMSIS", [(vendor("Core/Src/system_stm32h7xx.c"), 1)])
    add_group(groups, "Drivers/STM32H7xx_HAL_Driver",
              [(vendor(f"Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal{n}.c"), 1) for n in HAL])
    add_group(groups, "Middlewares/FreeRTOS", [
        (vendor("Middlewares/Third_Party/FreeRTOS/Source/tasks.c"), 1),
        (vendor("Middlewares/Third_Party/FreeRTOS/Source/queue.c"), 1),
        (vendor("Middlewares/Third_Party/FreeRTOS/Source/list.c"), 1),
        (vendor("Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM4F/port.c"), 1),
    ])

    OUT.parent.mkdir(parents=True, exist_ok=True)
    tree.write(OUT, encoding="UTF-8", xml_declaration=True)
    print(f"wrote {OUT} with {len(groups)} groups")


if __name__ == "__main__":
    main()
