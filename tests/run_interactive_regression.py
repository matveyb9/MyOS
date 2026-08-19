#!/usr/bin/env python3
import pathlib
import re
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
EDITOR_TEXT_PATH = "/users/myos/projects/editor-harness.txt"
EDITOR_SOURCE_PATH = "/users/myos/projects/editor-harness.mya"
EDITOR_ELF_PATH = "/users/myos/projects/editor-harness.elf"
EDITOR_APP_PATH = "/apps/editor-harness/main.elf"
SOURCE_PATH = "/temp/release-harness.mya"
ELF_PATH = "/users/myos/projects/release-harness.elf"
APP_PATH = "/apps/release-harness/main.elf"
FALSE_SOURCE_PATH = "/temp/f.mya"
FALSE_ELF_PATH = "/users/myos/projects/release-false.elf"
FALSE_APP_PATH = "/apps/release-false/main.elf"
NONZERO_SOURCE_PATH = "/temp/p.mya"
NONZERO_ELF_PATH = "/users/myos/projects/release-nonzero.elf"
NONZERO_APP_PATH = "/apps/release-nonzero/main.elf"
BACKWARD_SOURCE_PATH = "/temp/release-harness-backward.mya"
BACKWARD_ELF_PATH = "/users/myos/projects/release-harness-backward.elf"
MISSING_SET_SOURCE_PATH = "/temp/release-missing-set.mya"
MISSING_SET_ELF_PATH = "/users/myos/projects/release-missing-set.elf"
CONDITIONAL_BACKWARD_SOURCE_PATH = "/temp/release-conditional-backward.mya"
CONDITIONAL_BACKWARD_ELF_PATH = "/users/myos/projects/release-conditional-backward.elf"
INPUT_TIME_SOURCE_PATH = "/temp/release-input-time.mya"
INPUT_TIME_ELF_PATH = "/users/myos/projects/release-input-time.elf"
INPUT_TIME_APP_PATH = "/apps/release-input-time/main.elf"
TIME_LINE = re.compile(rb"(?m)^(?:[01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]\r?$")


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

    def run_with_input(self, line, character, marker):
        start = len(self.output)
        self.send(line + "\n")
        self.expect("Started process ", start)
        time.sleep(0.10)
        self.send(character)
        self.expect(marker, start)
        self.expect(PROMPT, start)
        return bytes(self.output[start:])

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

    def console_edit_and_save(self, path, content):
        start = len(self.output)
        self.send(f"edit {path}\n")
        self.expect("MYOS TEXT EDITOR", start)
        time.sleep(0.10)
        self.send(content + b"\x13")
        self.expect(f"edit: saved {len(content)} byte(s)", start)
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


def require_time_line(name, output):
    if TIME_LINE.search(output) is None:
        decoded = output[-2048:].decode("utf-8", errors="replace")
        raise RegressionFailure(f"{name}: native time output is not a valid HH:MM:SS line\n{decoded}")


def run_bios(image_path, work_dir):
    guest = Guest("bios", image_path, work_dir)
    try:
        guest.expect("[ok] Firmware: BIOS")
        guest.expect("[ok] Persistent storage mount: ready")
        guest.expect(PROMPT)
        guest.command(f"write {NOTE_PATH} base")
        guest.gui_edit_and_exit()
        guest.command(f"cat {NOTE_PATH}", "base!")
        guest.console_edit_and_save(EDITOR_TEXT_PATH, b"first\nsecond\n")
        text_start = len(guest.output)
        guest.command(f"cat {EDITOR_TEXT_PATH}", "first")
        text_output = guest.output[text_start:]
        if b"first\nsecond" not in text_output and b"first\r\nsecond" not in text_output:
            raise RegressionFailure(f"BIOS: editor text readback is not exact\n{guest._tail()}")
        editor_source = b"set 0\njump_if_zero done\nwrite \"bad\\n\"\nlabel done:\nwrite \"editor\\n\"\nexit 44\n"
        guest.console_edit_and_save(EDITOR_SOURCE_PATH, editor_source)
        guest.command(f"build {EDITOR_SOURCE_PATH} {EDITOR_ELF_PATH}", "exited with status 0")
        guest.command(f"install {EDITOR_ELF_PATH} {EDITOR_APP_PATH}", "exited with status 0")
        run_start = len(guest.output)
        guest.command("run editor-harness", "editor")
        run_output = guest.output[run_start:]
        if b"bad" in run_output or b"exited with status 44" not in run_output:
            raise RegressionFailure(f"BIOS: editor-authored program did not skip code or return status 44\n{guest._tail()}")
        guest.command(f"write {SOURCE_PATH} set 0;jump_if_zero done;write \"B\\n\";label done:;write \"Z\\n\";exit 7")
        guest.command(f"build {SOURCE_PATH} {ELF_PATH}", "exited with status 0")
        guest.command(f"install {ELF_PATH} {APP_PATH}", "exited with status 0")
        run_start = len(guest.output)
        guest.command("run release-harness", "Z")
        run_output = guest.output[run_start:]
        if b"B" in run_output or b"exited with status 7" not in run_output:
            raise RegressionFailure(f"BIOS: zero-true conditional program did not skip code or return status 7\n{guest._tail()}")
        guest.command(f"write {FALSE_SOURCE_PATH} set 9;jump_if_zero a;write \"F\\n\";jump e;label a:;write \"B\\n\";label e:;exit 8")
        guest.command(f"build {FALSE_SOURCE_PATH} {FALSE_ELF_PATH}", "exited with status 0")
        guest.command(f"install {FALSE_ELF_PATH} {FALSE_APP_PATH}", "exited with status 0")
        run_start = len(guest.output)
        guest.command("run release-false", "F")
        run_output = guest.output[run_start:]
        if b"B" in run_output or b"exited with status 8" not in run_output:
            raise RegressionFailure(f"BIOS: zero-false conditional program did not continue or return status 8\n{guest._tail()}")
        guest.command(f"write {NONZERO_SOURCE_PATH} set 5;jump_if_nonzero d;write \"B\\n\";label d:;write \"N\\n\";exit 9")
        guest.command(f"build {NONZERO_SOURCE_PATH} {NONZERO_ELF_PATH}", "exited with status 0")
        guest.command(f"install {NONZERO_ELF_PATH} {NONZERO_APP_PATH}", "exited with status 0")
        run_start = len(guest.output)
        guest.command("run release-nonzero", "N")
        run_output = guest.output[run_start:]
        if b"B" in run_output or b"exited with status 9" not in run_output:
            raise RegressionFailure(f"BIOS: nonzero-true conditional program did not skip code or return status 9\n{guest._tail()}")
        input_time_source = 'input;jump_if 65 a;write "B\\n";jump e;label a:;write "A\\n";label e:;time;exit 46'
        guest.command(f"write {INPUT_TIME_SOURCE_PATH} {input_time_source}")
        guest.command(f"build {INPUT_TIME_SOURCE_PATH} {INPUT_TIME_ELF_PATH}", "exited with status 0")
        guest.command(f"install {INPUT_TIME_ELF_PATH} {INPUT_TIME_APP_PATH}", "exited with status 0")
        input_true_output = guest.run_with_input("run release-input-time", b"A", "exited with status 46")
        if (b"B\n" in input_true_output or b"B\r\n" in input_true_output
                or (b"A\n" not in input_true_output and b"A\r\n" not in input_true_output)):
            raise RegressionFailure(f"BIOS: native input true branch did not match A\n{guest._tail()}")
        require_time_line("BIOS", input_true_output)
        input_false_output = guest.run_with_input("run release-input-time", b"B", "exited with status 46")
        if (b"A\n" in input_false_output or b"A\r\n" in input_false_output
                or (b"B\n" not in input_false_output and b"B\r\n" not in input_false_output)):
            raise RegressionFailure(f"BIOS: native input false branch did not select B\n{guest._tail()}")
        require_time_line("BIOS", input_false_output)
        diagnostic = "asm: syntax error; input/set must precede conditional jumps, labels need ':' and jumps must target a later label"
        guest.command(f"write {BACKWARD_SOURCE_PATH} label start:;write \"x\\n\";jump start;exit 0")
        guest.command(f"build {BACKWARD_SOURCE_PATH} {BACKWARD_ELF_PATH}", diagnostic)
        guest.command(f"write {MISSING_SET_SOURCE_PATH} jump_if 65 done;label done:;exit 0")
        guest.command(f"build {MISSING_SET_SOURCE_PATH} {MISSING_SET_ELF_PATH}", diagnostic)
        guest.command(f"write {CONDITIONAL_BACKWARD_SOURCE_PATH} label start:;set 1;jump_if 1 start;exit 0")
        guest.command(f"build {CONDITIONAL_BACKWARD_SOURCE_PATH} {CONDITIONAL_BACKWARD_ELF_PATH}", diagnostic)
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
        text_start = len(guest.output)
        guest.command(f"cat {EDITOR_TEXT_PATH}", "first")
        text_output = guest.output[text_start:]
        if b"first\nsecond" not in text_output and b"first\r\nsecond" not in text_output:
            raise RegressionFailure(f"UEFI: persisted editor text readback is not exact\n{guest._tail()}")
        run_start = len(guest.output)
        guest.command("run editor-harness", "editor")
        run_output = guest.output[run_start:]
        if b"bad" in run_output or b"exited with status 44" not in run_output:
            raise RegressionFailure(f"UEFI: persisted editor-authored program did not skip code or return status 44\n{guest._tail()}")
        run_start = len(guest.output)
        guest.command("run release-harness", "Z")
        run_output = guest.output[run_start:]
        if b"B" in run_output or b"exited with status 7" not in run_output:
            raise RegressionFailure(f"UEFI: persisted zero-true program did not skip code or return status 7\n{guest._tail()}")
        input_output = guest.run_with_input("run release-input-time", b"A", "exited with status 46")
        if (b"B\n" in input_output or b"B\r\n" in input_output
                or (b"A\n" not in input_output and b"A\r\n" not in input_output)):
            raise RegressionFailure(f"UEFI: persisted native input program did not match A\n{guest._tail()}")
        require_time_line("UEFI", input_output)
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
