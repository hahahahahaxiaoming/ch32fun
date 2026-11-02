"""WCH ISP console capture helpers adapted from UART_CH570/TOOLS/provision.py."""
import ctypes
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
from typing import List
ROOT = Path(__file__).resolve().parents[1]
ISP_DIR = Path(__file__).resolve().parent / "WCHISPTool_CH57x-59x"
ISP_EXE = ISP_DIR / "WCHISPTool_CH57x-59x.exe"
OUTPUT_DIR = ROOT / "build" / "isp_logs"
class ProvisionError(RuntimeError): pass
class WCHISPError(ProvisionError):
    def __init__(self, message, code=None):
        super().__init__(message)
        self.code=code
def decode_output(raw: bytes) -> str:
    for encoding in ("utf-8", "gb18030", "cp1252"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace")


def run_in_console(command: List[str], cwd: Path):
    """Run a legacy WriteConsole application and read its visible console rows."""
    if os.name != "nt" or not sys.stdout.isatty():
        return None

    from ctypes import wintypes

    class COORD(ctypes.Structure):
        _fields_ = (("X", ctypes.c_short), ("Y", ctypes.c_short))

    class SMALL_RECT(ctypes.Structure):
        _fields_ = (
            ("Left", ctypes.c_short), ("Top", ctypes.c_short),
            ("Right", ctypes.c_short), ("Bottom", ctypes.c_short),
        )

    class CONSOLE_SCREEN_BUFFER_INFO(ctypes.Structure):
        _fields_ = (
            ("dwSize", COORD), ("dwCursorPosition", COORD),
            ("wAttributes", wintypes.WORD), ("srWindow", SMALL_RECT),
            ("dwMaximumWindowSize", COORD),
        )

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetStdHandle.argtypes = (wintypes.DWORD,)
    kernel32.GetStdHandle.restype = wintypes.HANDLE
    kernel32.GetConsoleMode.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    kernel32.GetConsoleMode.restype = wintypes.BOOL
    kernel32.GetConsoleScreenBufferInfo.argtypes = (
        wintypes.HANDLE, ctypes.POINTER(CONSOLE_SCREEN_BUFFER_INFO)
    )
    kernel32.GetConsoleScreenBufferInfo.restype = wintypes.BOOL
    kernel32.ReadConsoleOutputCharacterW.argtypes = (
        wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, COORD,
        ctypes.POINTER(wintypes.DWORD),
    )
    kernel32.ReadConsoleOutputCharacterW.restype = wintypes.BOOL
    handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
    mode = wintypes.DWORD()
    if handle in (0, -1) or not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return None

    result = subprocess.run(command, cwd=cwd, check=False)

    info = CONSOLE_SCREEN_BUFFER_INFO()
    if not kernel32.GetConsoleScreenBufferInfo(handle, ctypes.byref(info)):
        return result.returncode, ""
    width = max(1, int(info.dwSize.X))
    end_row = max(0, int(info.dwCursorPosition.Y))
    start_row = max(0, end_row - 500)
    lines = []
    for row in range(start_row, end_row + 1):
        buffer = ctypes.create_unicode_buffer(width)
        count = wintypes.DWORD()
        if kernel32.ReadConsoleOutputCharacterW(
            handle, buffer, width, COORD(0, row), ctypes.byref(count)
        ):
            lines.append(buffer.value[:count.value].rstrip())
    output = "\n".join(lines)
    # Ignore older commands that may still be visible in the console history.
    last_header = output.rfind("====WchIsp")
    if last_header >= 0:
        output = output[last_header:]
    return result.returncode, output.strip()


def remove_if_present(path: Path):
    # Path.unlink(missing_ok=True) requires Python 3.8.
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def run_quiet_wchisp(command: List[str]):
    """Run WCHISP below pythonw so AttachConsole cannot find our BAT console."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix="wchisp_result_", suffix=".bin", dir=OUTPUT_DIR)
    os.close(fd)
    result_path = Path(name)
    remove_if_present(result_path)
    pythonw = Path(sys.executable).with_name("pythonw.exe")
    if not pythonw.is_file():
        pythonw = Path(sys.executable)
    try:
        helper = subprocess.run(
            [str(pythonw), str(Path(__file__).resolve()), "__quiet-wchisp",
             str(result_path), *command],
            cwd=ISP_DIR,
            check=False,
        )
        if not result_path.is_file():
            raise ProvisionError(f"后台 WCHISP 探测失败（辅助进程码 {helper.returncode}）")
        data = result_path.read_bytes()
        if len(data) < 4:
            raise ProvisionError("后台 WCHISP 探测没有返回完整结果")
        return struct.unpack("<i", data[:4])[0], decode_output(data[4:]).strip()
    finally:
        remove_if_present(result_path)


def quiet_wchisp_helper(result_path: Path, command: List[str]) -> int:
    try:
        result = subprocess.run(
            command,
            cwd=ISP_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        result_path.write_bytes(struct.pack("<i", result.returncode) + result.stdout)
        return 0
    except Exception as exc:
        result_path.write_bytes(struct.pack("<i", 1) + str(exc).encode("utf-8", "replace"))
        return 1


def run_isp(arguments: List[str], quiet: bool = False) -> str:
    if os.name != "nt":
        raise ProvisionError("WCHISPTool_CH57x-59x.exe 只能在 Windows 上运行")
    if not ISP_EXE.is_file():
        raise ProvisionError(f"找不到下载工具：{ISP_EXE}")
    command = [str(ISP_EXE), *arguments]
    console_result = run_quiet_wchisp(command) if quiet else run_in_console(command, ISP_DIR)
    if console_result is not None:
        returncode, output = console_result
    else:
        result = subprocess.run(
            command,
            cwd=ISP_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        returncode = result.returncode
        output = decode_output(result.stdout).strip()
        if output and not quiet:
            print(output)
    failure = None
    # Console screen capture inserts a newline when a long JSON line wraps.
    flat_output = output.replace("\r", "").replace("\n", "")
    for match in re.finditer(r"\{[^{}]*\}", flat_output):
        try:
            item = json.loads(match.group(0))
        except json.JSONDecodeError:
            continue
        status = str(item.get("Status", "")).lower()
        code = item.get("Code")
        if (isinstance(code, int) and code != 0) or status in (
            "fail", "failed", "failure", "error"
        ):
            failure = item
    if returncode != 0 or failure:
        detail = json.dumps(failure, ensure_ascii=False) if failure else output
        code = failure.get("Code") if failure else None
        raise WCHISPError(f"WCHISPTool 执行失败（进程码 {returncode}）：{detail}", code)
    return output


if __name__ == "__main__":
    if len(sys.argv)>2 and sys.argv[1]=="__quiet-wchisp":
        raise SystemExit(quiet_wchisp_helper(Path(sys.argv[2]),sys.argv[3:]))
    raise SystemExit("This module is used by download.py")
