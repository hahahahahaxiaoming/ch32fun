"""USB ISP download of one explicitly selected chip/mode; no build or driver install."""
import argparse
import math
from pathlib import Path
import tempfile
import time
from isp_runner import ROOT, ISP_EXE, ProvisionError, WCHISPError, run_isp


def firmware_path(chip, mode):
    if chip not in ('CH570', 'CH591', 'CH592') or mode not in ('uart', 'rs485'):
        raise ValueError('Unsupported chip or mode')
    firmware_chip = 'CH591' if chip == 'CH592' else chip
    return ROOT / 'build' / firmware_chip / mode / ('usb_' + mode + '.bin')


def config_text(chip, firmware):
    if chip not in ('CH570', 'CH591', 'CH592'):
        raise ValueError('Unsupported chip')
    # Same WCH chip selection/options as the reference UART_CH570 project.
    line, kind = {'CH570': (3, 112), 'CH591': (12, 145), 'CH592': (12, 146)}[chip]
    boot, reset, reset_num = ('PB22', 'PA8', 0) if chip == 'CH570' else ('Boot0', '.', 1)
    return f'''[Public]
MCUName={chip}
bMCULine={line}
bMCUType={kind}
DataFlashFile=.
swzUserFile1={firmware.resolve()}
swzUserFile2=.
swzUserFile3=.
swzUserFile4=.
swzUserFile5=.
DataFlashFileSel=0
IsUserFile1Sel=1
IsUserFile2Sel=0
IsUserFile3Sel=0
IsUserFile4Sel=0
IsUserFile5Sel=0
[CH57x-58xUICfg]
bDnInterType=0
Baud=115200
DwnldCfgPin={boot}
BootPinNum=1
WProtectAddr={0 if chip == 'CH570' else '.'}
IsCodeProtect=1
IsRSTAsInputPin=0
ExtRSTPinSel={reset}
ExtRSTPinSelNum={reset_num}
IndependentWDGEn=0
IsSerialNoBtnDwnld=0
IsClearDataFlash=0
IsEraseAllCFlash=0
IsAfterDownRest=1
bVerifyType=1
'''


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chip', choices=('CH570', 'CH591', 'CH592'), required=True)
    parser.add_argument('--mode', choices=('uart', 'rs485'), required=True)
    parser.add_argument('--wait', type=int, default=120)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args(argv)
    firmware = firmware_path(args.chip, args.mode)
    if not firmware.is_file():
        raise ProvisionError(f'Missing {firmware}. Run make first.')
    config = config_text(args.chip, firmware)
    print(f'Target: {args.chip} / {args.mode}\nFirmware: {firmware}', flush=True)
    if args.dry_run:
        print(config)
        return 0
    if not ISP_EXE.is_file():
        raise ProvisionError(f'Missing ISP tool: {ISP_EXE}')
    print('Hold BOOT, plug in USB, then release BOOT. Connect only ONE ISP device.', flush=True)
    with tempfile.TemporaryDirectory(prefix='usb_bridge_isp_') as directory:
        cfg = Path(directory) / 'download.ini'
        cfg.write_text(config, encoding='utf-8')
        deadline = time.monotonic() + max(0, args.wait)
        while True:
            remaining = max(0, math.ceil(deadline-time.monotonic()))
            print(f'\rWaiting for {args.chip} USB ISP... {remaining}s   ', end='', flush=True)
            try:
                run_isp(['-c', str(cfg), '-u', 'get'], quiet=True)
                print()
                break
            except WCHISPError as exc:
                if exc.code != 5 or time.monotonic() >= deadline:
                    print()
                    raise
                time.sleep(1)
        run_isp(['-c', str(cfg), '-o', 'download', '-f', str(firmware)])
    print('SUCCESS: downloaded and requested application start.')
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ProvisionError, OSError, UnicodeError) as exc:
        print(f'ERROR: {exc}', flush=True)
        raise SystemExit(1)
    except KeyboardInterrupt:
        raise SystemExit(130)
