"""Validate a CH570 bridge APP or combine it with the bootloader for factory programming."""

import argparse
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
APP_ADDRESS = 0x1000
APP_VID = 0x1A86
APP_PIDS = {"uart": 0x0001, "rs485": 0x0002}
APP_FLASH_LIMIT = 236 * 1024
APP_VECTOR_TAG = 0xF3F9BDA9


class UpgradeError(RuntimeError):
    pass


def read_firmware(chip, mode):
    firmware = ROOT / "build" / chip / mode / f"usb_{mode}.bin"
    if not firmware.is_file():
        raise UpgradeError(f"Missing {firmware}. Run make first.")
    image = firmware.read_bytes()
    entry = int.from_bytes(image[:4], "little")
    has_entry_jump = (entry & 0x7F) == 0x6F or (entry & 0xE003) == 0xA001
    if (not 24 <= len(image) <= APP_FLASH_LIMIT or not has_entry_jump
            or image[0x14:0x18] != struct.pack("<I", APP_VECTOR_TAG)):
        raise UpgradeError(f"{firmware} is not a CH570 APP image at 0x1000.")
    # Check the compiled USB device descriptor even if the .bin has been renamed.
    descriptor_prefixes = (bytes.fromhex("12 01 00 02 02 02 00 40"),
                           bytes.fromhex("12 01 00 02 EF 02 01 40"))
    matching_ids = [pid for pid in APP_PIDS.values()
                    if any(prefix + struct.pack("<HH", APP_VID, pid) in image
                           for prefix in descriptor_prefixes)]
    if matching_ids != [APP_PIDS[mode]]:
        raise UpgradeError(f"{firmware} does not contain the expected {mode} USB identity.")
    return firmware, image


def create_factory_image(chip, firmware, image):
    boot = ROOT.parent / "usb_bootloader" / "build" / chip / "usb_bootloader.bin"
    if not boot.is_file():
        raise UpgradeError(f"Missing {boot}. Build the USB bootloader first.")
    boot_image = boot.read_bytes()
    if not 0 < len(boot_image) <= APP_ADDRESS:
        raise UpgradeError(f"Invalid bootloader size: {len(boot_image)} bytes.")
    output = firmware.with_name(f"{firmware.stem}_{chip}_factory.bin")
    output.write_bytes(boot_image.ljust(APP_ADDRESS, b"\xff") + image)
    print(f"Factory image for {chip}: {output} (program at 0x0000)")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chip", choices=("CH570",), default="CH570")
    parser.add_argument("--mode", choices=APP_PIDS, required=True)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--dry-run", action="store_true")
    action.add_argument("--factory-image", action="store_true", help="Combine bootloader and APP for first programming")
    args = parser.parse_args(argv)
    firmware, image = read_firmware(args.chip, args.mode)
    print(f"Target: {args.chip} / {args.mode} (1A86:{APP_PIDS[args.mode]:04X})\n"
          f"APP at 0x1000: {firmware}", flush=True)
    if args.factory_image:
        create_factory_image(args.chip, firmware, image)


if __name__ == "__main__":
    try:
        main()
    except (UpgradeError, OSError) as exc:
        print(f"ERROR: {exc}", flush=True)
        raise SystemExit(1)
    except KeyboardInterrupt:
        raise SystemExit(130)
