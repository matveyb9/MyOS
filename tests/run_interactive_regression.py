#!/usr/bin/env python3
import json
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
ARGS_SOURCE_PATH = "/temp/release-args.mya"
ARGS_ELF_PATH = "/users/myos/projects/release-args.elf"
ARGS_APP_PATH = "/apps/release-args/main.elf"
COPY_SOURCE_PATH = "/users/myos/files/cp-harness-source.txt"
COPY_TARGET_PATH = "/users/myos/files/cp-harness-target.txt"
TIME_LINE = re.compile(rb"(?m)^(?:[01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]\r?$")


class RegressionFailure(RuntimeError):
    pass


class Guest:
    def __init__(self, name, image_path, work_dir, uefi_code=None, uefi_vars=None):
        self.name = name
        self.socket_path = str(work_dir / f"{name}.serial.sock")
        self.qmp_socket_path = str(work_dir / f"{name}.qmp.sock")
        self.work_dir = work_dir
        self.output = bytearray()
        self.connection = None
        self.qmp_connection = None
        self.qmp_buffer = bytearray()
        command = [
            "qemu-system-x86_64",
            "-machine", "q35",
            "-m", "256M",
            "-drive", f"if=ide,format=raw,file={image_path}",
            "-boot", "c",
            "-serial", f"unix:{self.socket_path},server=on,wait=off",
            "-qmp", f"unix:{self.qmp_socket_path},server=on,wait=off",
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
        self._connect_qmp()

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

    def _connect_qmp(self):
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RegressionFailure(f"{self.name}: QEMU stopped before QMP socket became available")
            candidate = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                candidate.connect(self.qmp_socket_path)
                candidate.setblocking(False)
                self.qmp_connection = candidate
                greeting = self._qmp_read_message()
                if "QMP" not in greeting:
                    raise RegressionFailure(f"{self.name}: QMP greeting is invalid: {greeting!r}")
                self._qmp_request("qmp_capabilities")
                return
            except FileNotFoundError:
                candidate.close()
            except ConnectionRefusedError:
                candidate.close()
            time.sleep(0.05)
        raise RegressionFailure(f"{self.name}: cannot connect to QMP socket")

    def _qmp_read_message(self, timeout=10.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            newline = self.qmp_buffer.find(b"\n")
            if newline >= 0:
                raw = bytes(self.qmp_buffer[:newline])
                del self.qmp_buffer[:newline + 1]
                if raw:
                    return json.loads(raw.decode("utf-8"))
            ready, _, _ = select.select([self.qmp_connection], [], [], 0.20)
            if ready:
                chunk = self.qmp_connection.recv(4096)
                if not chunk:
                    raise RegressionFailure(f"{self.name}: QMP connection closed")
                self.qmp_buffer.extend(chunk)
        raise RegressionFailure(f"{self.name}: QMP response timeout")

    def _qmp_request(self, command, arguments=None):
        request = {"execute": command}
        if arguments is not None:
            request["arguments"] = arguments
        self.qmp_connection.sendall(json.dumps(request).encode("utf-8") + b"\n")
        while True:
            response = self._qmp_read_message()
            if "return" in response:
                return response["return"]
            if "error" in response:
                raise RegressionFailure(f"{self.name}: QMP {command} failed: {response['error']}")

    def qmp_screendump(self, stem):
        path = self.work_dir / f"{self.name}-{stem}.ppm"
        if path.exists():
            path.unlink()
        self._qmp_request("screendump", {"filename": str(path)})
        if not path.is_file():
            raise RegressionFailure(f"{self.name}: QMP did not create {path.name}")
        return path.read_bytes()

    def require_framebuffer_transition(self, before, after, label):
        changed = sum(left != right for left, right in zip(before, after))
        if len(before) != len(after) or changed < 4096:
            raise RegressionFailure(f"{self.name}: {label} did not produce the expected framebuffer transition")

    def ppm_pixel(self, image, x, y):
        header_end = image.find(b"\n255\n")
        if header_end < 0:
            raise RegressionFailure(f"{self.name}: QMP screendump is not a binary PPM image")
        header = image[:header_end].split()
        if len(header) != 3 or header[0] != b"P6":
            raise RegressionFailure(f"{self.name}: QMP screendump header is invalid")
        width = int(header[1])
        payload_start = header_end + len(b"\n255\n")
        offset = payload_start + ((y * width) + x) * 3
        if offset + 3 > len(image):
            raise RegressionFailure(f"{self.name}: PPM pixel coordinate is outside the screendump")
        return image[offset:offset + 3]

    def qmp_move(self, delta_x=0, delta_y=0):
        for axis, delta in (("x", delta_x), ("y", delta_y)):
            remaining = delta
            while remaining != 0:
                step = min(100, remaining) if remaining > 0 else max(-100, remaining)
                self._qmp_request("input-send-event", {"events": [
                    {"type": "rel", "data": {"axis": axis, "value": step}},
                ]})
                remaining -= step
                time.sleep(0.05)

    def qmp_left_click(self):
        self.qmp_move(delta_x=1)
        time.sleep(0.05)
        self._qmp_request("input-send-event", {"events": [
            {"type": "btn", "data": {"down": True, "button": "left"}},
        ]})
        time.sleep(0.05)
        self._qmp_request("input-send-event", {"events": [
            {"type": "btn", "data": {"down": False, "button": "left"}},
        ]})

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

    def _drain_output(self):
        while True:
            ready, _, _ = select.select([self.connection], [], [], 0.0)
            if not ready:
                return
            chunk = self.connection.recv(4096)
            if not chunk:
                raise RegressionFailure(f"{self.name}: serial connection closed while draining output\n{self._tail()}")
            self.output.extend(chunk)

    def send(self, payload):
        if isinstance(payload, str):
            payload = payload.encode("utf-8")
        pending = memoryview(payload)
        while len(pending) != 0:
            self._drain_output()
            try:
                written = self.connection.send(pending)
            except BlockingIOError:
                select.select([self.connection], [self.connection], [], 0.20)
                continue
            if written == 0:
                raise RegressionFailure(f"{self.name}: serial connection closed while sending input\n{self._tail()}")
            pending = pending[written:]

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
        for index in range(len(content)):
            self.send(content[index:index + 1])
            time.sleep(0.015)
        self.send(b"\x13")
        self.expect(f"edit: saved {len(content)} byte(s)", start, timeout=60.0)
        self.expect("exited with status 0", start, timeout=60.0)
        self.expect(PROMPT, start)

    def gui_open_and_exit(self):
        start = len(self.output)
        self.send(f"startgui {NOTE_PATH}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.send(b"Q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_desktop_navigate_and_exit(self, command="startgui"):
        start = len(self.output)
        self.send(command + "\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        for key in (b"m", b"h", b"n", b"h", b"Q"):
            self.send(key)
            time.sleep(0.10)
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_mouse_notes_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        before = self.qmp_screendump("desktop-before-click")
        self.qmp_left_click()
        time.sleep(0.25)
        after = self.qmp_screendump("desktop-after-click")
        self.require_framebuffer_transition(before, after, "launcher NOTES click")
        self.qmp_move(delta_x=600, delta_y=390)
        self.qmp_left_click()
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_mouse_window_chrome_and_exit(self):
        start = len(self.output)
        self.send(f"startgui {NOTE_PATH}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        before = self.qmp_screendump("window-chrome-before")
        self.qmp_move(delta_x=151, delta_y=329)
        self.qmp_left_click()
        time.sleep(0.25)
        system_closed = self.qmp_screendump("window-chrome-system-closed")
        self.require_framebuffer_transition(before, system_closed, "SYSTEM window close")
        self.qmp_move(delta_x=-134, delta_y=-39)
        self.qmp_left_click()
        time.sleep(0.25)
        monitor_raised = self.qmp_screendump("window-chrome-monitor-raised")
        self.require_framebuffer_transition(system_closed, monitor_raised, "MONITOR title-bar raise")
        self.qmp_move(delta_x=385, delta_y=1)
        self.qmp_left_click()
        time.sleep(0.25)
        monitor_closed = self.qmp_screendump("window-chrome-monitor-closed")
        self.require_framebuffer_transition(monitor_raised, monitor_closed, "MONITOR window close")
        self.qmp_move(delta_x=20, delta_y=-98)
        self.qmp_left_click()
        time.sleep(0.25)
        launcher = self.qmp_screendump("window-chrome-viewer-closed")
        self.require_framebuffer_transition(monitor_closed, launcher, "viewer window close")
        self.qmp_move(delta_x=174, delta_y=191)
        self.qmp_left_click()
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_mouse_editor_close_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        launcher = self.qmp_screendump("editor-close-launcher")
        self.qmp_move(delta_x=115)
        self.qmp_left_click()
        time.sleep(0.25)
        editor = self.qmp_screendump("editor-close-editor")
        self.require_framebuffer_transition(launcher, editor, "launcher EDIT NOTE click")
        self.qmp_move(delta_x=309, delta_y=193)
        self.qmp_left_click()
        time.sleep(0.25)
        viewer = self.qmp_screendump("editor-close-viewer")
        editor_title = self.ppm_pixel(editor, 338, 210)
        viewer_title = self.ppm_pixel(viewer, 338, 210)
        if editor_title == viewer_title:
            raise RegressionFailure(f"{self.name}: editor close did not cancel to the viewer")
        self.qmp_move(delta_x=174, delta_y=191)
        self.qmp_left_click()
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def close(self):
        if self.qmp_connection is not None:
            self.qmp_connection.close()
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


def require_native_line(name, output, expected):
    accepted = (expected + b"\n", expected + b"\r\n")
    if accepted[0] not in output and accepted[1] not in output:
        decoded = output[-2048:].decode("utf-8", errors="replace")
        raise RegressionFailure(f"{name}: native output lacks {expected!r} as a complete line\n{decoded}")


def run_bios(image_path, work_dir):
    guest = Guest("bios", image_path, work_dir)
    try:
        guest.expect("[ok] Firmware: BIOS")
        guest.expect("[ok] Persistent storage mount: ready")
        guest.expect(PROMPT)
        guest.command(f"write {NOTE_PATH} base")
        guest.gui_edit_and_exit()
        guest.command(f"cat {NOTE_PATH}", "base!")
        guest.gui_desktop_navigate_and_exit()
        guest.gui_mouse_notes_and_exit()
        guest.gui_mouse_window_chrome_and_exit()
        guest.gui_mouse_editor_close_and_exit()
        guest.gui_desktop_navigate_and_exit("startgui home")
        guest.console_edit_and_save(EDITOR_TEXT_PATH, b"first\nsecond\n")
        text_start = len(guest.output)
        guest.command(f"cat {EDITOR_TEXT_PATH}", "first")
        text_output = guest.output[text_start:]
        if b"first\nsecond" not in text_output and b"first\r\nsecond" not in text_output:
            raise RegressionFailure(f"BIOS: editor text readback is not exact\n{guest._tail()}")
        copy_payload = b"copy:" + b"x" * 300
        guest.console_edit_and_save(COPY_SOURCE_PATH, copy_payload)
        copy_start = len(guest.output)
        guest.command(f"run cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "exited with status 0")
        copy_output = bytes(guest.output[copy_start:])
        if b"Copied 305 byte(s)" not in copy_output:
            raise RegressionFailure(f"BIOS: SDK cp did not report a 305-byte copy\n{guest._tail()}")
        copy_read_start = len(guest.output)
        guest.command(f"cat {COPY_TARGET_PATH}", "copy:")
        copy_read_output = bytes(guest.output[copy_read_start:])
        if copy_payload not in copy_read_output:
            raise RegressionFailure(f"BIOS: SDK cp target readback is not exact\n{guest._tail()}")
        guest.command(f"run cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "target must not exist")
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
        args_source = 'write "[";args;write "]\\n";time;exit 47'
        guest.command(f"write {ARGS_SOURCE_PATH} {args_source}")
        guest.command(f"build {ARGS_SOURCE_PATH} {ARGS_ELF_PATH}", "exited with status 0")
        guest.command(f"install {ARGS_ELF_PATH} {ARGS_APP_PATH}", "exited with status 0")
        args_empty_start = len(guest.output)
        guest.command("run release-args", "exited with status 47")
        args_empty_output = bytes(guest.output[args_empty_start:])
        require_native_line("BIOS empty native args", args_empty_output, b"[]")
        require_time_line("BIOS empty native args", args_empty_output)
        args_output_start = len(guest.output)
        guest.command("run release-args alpha beta", "exited with status 47")
        args_output = bytes(guest.output[args_output_start:])
        require_native_line("BIOS native args", args_output, b"[alpha beta]")
        require_time_line("BIOS native args", args_output)
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
        copy_payload = b"copy:" + b"x" * 300
        copy_read_start = len(guest.output)
        guest.command(f"cat {COPY_TARGET_PATH}", "copy:")
        copy_read_output = bytes(guest.output[copy_read_start:])
        if copy_payload not in copy_read_output:
            raise RegressionFailure(f"UEFI: persisted SDK cp target readback is not exact\n{guest._tail()}")
        guest.command(f"run cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "target must not exist")
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
        args_output_start = len(guest.output)
        guest.command("run release-args ovmf args", "exited with status 47")
        args_output = bytes(guest.output[args_output_start:])
        require_native_line("UEFI persisted native args", args_output, b"[ovmf args]")
        require_time_line("UEFI persisted native args", args_output)
        guest.gui_open_and_exit()
        guest.gui_desktop_navigate_and_exit()
        guest.gui_mouse_notes_and_exit()
        guest.gui_mouse_window_chrome_and_exit()
        guest.gui_mouse_editor_close_and_exit()
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
