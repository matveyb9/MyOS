#!/usr/bin/env python3
import pathlib
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import time


PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
PROMPT = b"[myos]$ "
NOTE_PATH = "/users/myos/files/notes/release-harness.txt"
SOURCE_PATH = "/users/myos/projects/release-harness.mya"
ELF_PATH = "/users/myos/projects/release-harness.elf"
APP_PATH = "/apps/release-harness/main.elf"


class RegressionFailure(RuntimeError):
    pass


class Guest:
    def __init__(self, name, image_path, work_dir, uefi_code=None, uefi_vars=None):
        self.name = name
        self.socket_path = str(work_dir / f"{name}.serial.sock")
        self.output = bytearray()
        self.connection = None
        command = [
            "qemu-system-x86_64",
            "-machine", "q35",
            "-m", "256M",
            "-drive", f"if=ide,format=raw,file={image_path}",
            "-boot", "c",
            "-serial", f"unix:{self.socket_path},server=on,wait=off",
            "-display", "none",
            "-no-reboot",
            "-no-shutdown",
        ]
        if uefi_code is not None and uefi_vars is not None:
            command[1:1] = [
                "-drive", f"if=pflash,format=raw,readonly=on,file={uefi_code}",
                "-drive", f"if=pflash,format=raw,file={uefi_vars}",
            ]
        self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self._connect()

    def _connect(self):
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RegressionFailure(f"{self.name}: QEMU stopped before serial socket became available")
            candidate = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                candidate.connect(self.socket_path)
                candidate.setblocking(False)
                self.connection = candidate
                return
            except FileNotFoundError:
                candidate.close()
            except ConnectionRefusedError:
                candidate.close()
            time.sleep(0.05)
        raise RegressionFailure(f"{self.name}: cannot connect to QEMU serial socket")

    def _tail(self):
        return self.output[-4096:].decode("utf-8", errors="replace")

    def expect(self, marker, start=None, timeout=20.0):
        if isinstance(marker, str):
            marker = marker.encode("utf-8")
        if start is None:
            start = len(self.output)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if marker in self.output[start:]:
                return
            if self.process.poll() is not None:
                raise RegressionFailure(f"{self.name}: QEMU stopped while waiting for {marker!r}\n{self._tail()}")
            ready, _, _ = select.select([self.connection], [], [], 0.20)
            if ready:
                chunk = self.connection.recv(4096)
                if not chunk:
                    raise RegressionFailure(f"{self.name}: serial connection closed while waiting for {marker!r}\n{self._tail()}")
                self.output.extend(chunk)
        raise RegressionFailure(f"{self.name}: timeout waiting for {marker!r}\n{self._tail()}")

    def send(self, payload):
        if isinstance(payload, str):
            payload = payload.encode("utf-8")
        self.connection.sendall(payload)

    def command(self, line, marker=None):
        start = len(self.output)
        self.send(line + "\n")
        if marker is not None:
            self.expect(marker, start)
        self.expect(PROMPT, start)

    def gui_edit_and_exit(self):
        start = len(self.output)
        self.send(f"startgui {NOTE_PATH}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.send(b"E!\x13Q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_open_and_exit(self):
        start = len(self.output)
        self.send(f"startgui {NOTE_PATH}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.send(b"Q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def close(self):
        if self.connection is not None:
            self.connection.close()
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3.0)


def require_file(path):
    if not pathlib.Path(path).is_file():
        raise RegressionFailure(f"required file is missing: {path}")


def run_bios(image_path, work_dir):
    guest = Guest("bios", image_path, work_dir)
    try:
        guest.expect("[ok] Firmware: BIOS")
        guest.expect("[ok] Persistent storage mount: ready")
        guest.expect(PROMPT)
        guest.command(f"write {NOTE_PATH} base")
        guest.gui_edit_and_exit()
        guest.command(f"cat {NOTE_PATH}", "base!")
        guest.command(f"write {SOURCE_PATH} write \"native\\n\"; exit 7")
        guest.command(f"build {SOURCE_PATH} {ELF_PATH}", "exited with status 0")
        guest.command(f"install {ELF_PATH} {APP_PATH}", "exited with status 0")
        guest.command("run release-harness", "native")
        if b"exited with status 7" not in guest.output:
            raise RegressionFailure(f"BIOS: native program exit status 7 missing\n{guest._tail()}")
    finally:
        guest.close()


def run_uefi(image_path, work_dir, code_path, vars_source):
    vars_copy = work_dir / "OVMF_VARS.fd"
    shutil.copy2(vars_source, vars_copy)
    guest = Guest("uefi", image_path, work_dir, code_path, vars_copy)
    try:
        guest.expect("[ok] Firmware: UEFI x86_64")
        guest.expect("[ok] Persistent storage mount: ready")
        guest.expect(PROMPT)
        guest.command(f"cat {NOTE_PATH}", "base!")
        guest.command("run release-harness", "native")
        if b"exited with status 7" not in guest.output:
            raise RegressionFailure(f"UEFI: persisted native program exit status 7 missing\n{guest._tail()}")
        guest.gui_open_and_exit()
    finally:
        guest.close()


def main():
    image_source = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else PROJECT_ROOT / "myos.img").resolve()
    ovmf_code = pathlib.Path("/usr/share/OVMF/OVMF_CODE_4M.fd")
    ovmf_vars = pathlib.Path("/usr/share/OVMF/OVMF_VARS_4M.fd")
    require_file(image_source)
    require_file(ovmf_code)
    require_file(ovmf_vars)

    with tempfile.TemporaryDirectory(prefix="myos-interactive-regression-") as temporary:
        work_dir = pathlib.Path(temporary)
        test_image = work_dir / "myos-regression.img"
        shutil.copy2(image_source, test_image)
        run_bios(test_image, work_dir)
        print("interactive regression: BIOS GUI/native workflow passed")
        run_uefi(test_image, work_dir, ovmf_code, ovmf_vars)
        print("interactive regression: UEFI persistence workflow passed")


if __name__ == "__main__":
    try:
        main()
    except RegressionFailure as error:
        print(f"interactive regression: FAILED: {error}", file=sys.stderr)
        sys.exit(1)
