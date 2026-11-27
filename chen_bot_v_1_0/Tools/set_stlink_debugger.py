"""Force every project's debugger to ST-Link V2 (ST-LINKIII-KEIL_SWO).

uVision creates a fresh .uvoptx with ULINK defaults when a project is opened for
the first time, which silently ignores the ST-Link selected in the template.
This script makes the choice explicit and idempotent:

  * <nTsel>5</nTsel>                       ST-Link debugger
  * <pMon>STLink\\ST-LINKIII-KEIL_SWO.dll</pMon>
  * a ST-LINKIII-KEIL_SWO entry in TargetDriverDllRegistry carrying the
    1 MB flash algorithm for STM32H723VGTx

Usage:
    python Tools/set_stlink_debugger.py ../MDK-ARM/chen_bot_v_1_0.uvoptx [...]
    python Tools/set_stlink_debugger.py --all
"""
import re
import sys
from pathlib import Path

DEV = Path(__file__).resolve().parents[1]
ROOT = DEV.parent

DEFAULT_PROJECTS = [
    DEV / "MDK-ARM/chen_bot_v_1_0.uvoptx",
    ROOT / "soft1/development/Firmware/MDK-ARM/MC02_LCD_Smoke.uvoptx",
]

PILOT = "ST-LINKIII-KEIL_SWO"
ENTRY_RE = re.compile(
    r"<SetRegEntry>\s*<Number>\d+</Number>\s*<Key>ST-LINKIII-KEIL_SWO</Key>\s*"
    r"<Name>(?P<name>.*?)</Name>\s*</SetRegEntry>", re.S)


def pilot_entry(projects):
    """Reuse the already tuned ST-Link entry from any project that has one."""
    for path in projects:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        match = ENTRY_RE.search(text)
        if match:
            return match.group("name")
    raise SystemExit("no existing ST-Link entry to copy; set one in uVision first")


def patch(path: Path, name: str) -> str:
    text = path.read_text(encoding="utf-8", errors="replace")
    before = text

    text = re.sub(r"<nTsel>\d+</nTsel>", "<nTsel>5</nTsel>", text)
    text = re.sub(r"<pMon>.*?</pMon>", r"<pMon>STLink\\ST-LINKIII-KEIL_SWO.dll</pMon>", text)

    if f"<Key>{PILOT}</Key>" not in text:
        entry = ("        <SetRegEntry>\n"
                 "          <Number>0</Number>\n"
                 f"          <Key>{PILOT}</Key>\n"
                 f"          <Name>{name}</Name>\n"
                 "        </SetRegEntry>\n")
        text = text.replace("      </TargetDriverDllRegistry>",
                            entry + "      </TargetDriverDllRegistry>", 1)

    if text != before:
        path.write_text(text, encoding="utf-8")
        return "updated"
    return "already ST-Link"


def main(argv):
    projects = DEFAULT_PROJECTS if (not argv or argv[0] == "--all") else [Path(a) for a in argv]
    name = pilot_entry(DEFAULT_PROJECTS)
    for path in projects:
        if not path.exists():
            print(f"skip (missing): {path}")
            continue
        print(f"{patch(path, name):16} {path.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
