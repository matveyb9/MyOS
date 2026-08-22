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
DEFAULT_GUI_NOTE_PATH = "/users/myos/files/notes/note"
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
VARIABLE_SOURCE_PATH = "/temp/release-vars.mya"
VARIABLE_ELF_PATH = "/users/myos/projects/release-vars.elf"
VARIABLE_APP_PATH = "/apps/release-vars/main.elf"
INVALID_VARIABLE_SOURCE_PATH = "/temp/release-vars-invalid.mya"
INVALID_VARIABLE_ELF_PATH = "/users/myos/projects/release-vars-invalid.elf"
ARITHMETIC_SOURCE_PATH = "/temp/release-arithmetic.mya"
ARITHMETIC_ELF_PATH = "/users/myos/projects/release-arithmetic.elf"
ARITHMETIC_APP_PATH = "/apps/release-arithmetic/main.elf"
INVALID_ARITHMETIC_SOURCE_PATH = "/temp/release-arithmetic-invalid.mya"
INVALID_ARITHMETIC_ELF_PATH = "/users/myos/projects/release-arithmetic-invalid.elf"
MULDIV_SOURCE_PATH = "/temp/release-muldiv.mya"
MULDIV_ELF_PATH = "/users/myos/projects/release-muldiv.elf"
MULDIV_APP_PATH = "/apps/release-muldiv/main.elf"
BITWISE_SOURCE_PATH = "/temp/release-bitwise.mya"
BITWISE_ELF_PATH = "/users/myos/projects/release-bitwise.elf"
BITWISE_APP_PATH = "/apps/release-bitwise/main.elf"
INVALID_BITWISE_SOURCE_PATH = "/temp/release-bitwise-invalid.mya"
INVALID_BITWISE_ELF_PATH = "/users/myos/projects/release-bitwise-invalid.elf"
XOR_SOURCE_PATH = "/temp/release-xor.mya"
XOR_ELF_PATH = "/users/myos/projects/release-xor.elf"
XOR_APP_PATH = "/apps/release-xor/main.elf"
INVALID_XOR_SOURCE_PATH = "/temp/release-xor-invalid.mya"
INVALID_XOR_ELF_PATH = "/users/myos/projects/release-xor-invalid.elf"
INVALID_XOR_BOUND_SOURCE_PATH = "/temp/release-xor-bound-invalid.mya"
INVALID_XOR_BOUND_ELF_PATH = "/users/myos/projects/release-xor-bound-invalid.elf"
SHIFT_SOURCE_PATH = "/temp/release-shift.mya"
SHIFT_ELF_PATH = "/users/myos/projects/release-shift.elf"
SHIFT_APP_PATH = "/apps/release-shift/main.elf"
INVALID_SHIFT_SOURCE_PATH = "/temp/release-shift-invalid.mya"
INVALID_SHIFT_ELF_PATH = "/users/myos/projects/release-shift-invalid.elf"
INVALID_DIVISION_SOURCE_PATH = "/temp/release-division-invalid.mya"
INVALID_DIVISION_ELF_PATH = "/users/myos/projects/release-division-invalid.elf"
CMP_SOURCE_PATH = "/temp/release-cmp.mya"
CMP_ELF_PATH = "/users/myos/projects/release-cmp.elf"
CMP_APP_PATH = "/apps/release-cmp/main.elf"
INVALID_CMP_SOURCE_PATH = "/temp/release-cmp-invalid.mya"
INVALID_CMP_ELF_PATH = "/users/myos/projects/release-cmp-invalid.elf"
INVALID_CMP_SLOT_SOURCE_PATH = "/temp/release-cmp-slot-invalid.mya"
INVALID_CMP_SLOT_ELF_PATH = "/users/myos/projects/release-cmp-slot-invalid.elf"
SDK_WRITE_EXAMPLE_PATH = "/system/core/examples/sdk/write.elf"
SDK_WRITE_APP_PATH = "/apps/sdk-write/main.elf"
SDK_WRITE_BIOS_TARGET = "/users/myos/files/sdk-write-bios.txt"
SDK_WRITE_UEFI_TARGET = "/users/myos/files/sdk-write-uefi.txt"
SDK_WRITE_PAYLOAD = b"sdk-write: persistent VFS example\n"
COPY_SOURCE_PATH = "/users/myos/files/cp-harness-source.txt"
COPY_TARGET_PATH = "/users/myos/files/cp-harness-target.txt"
WC_WORD_PATH = "/users/myos/files/wc-harness.txt"
GREP_MATCH_PATH = "/users/myos/files/grep-harness.txt"
GUI_NEW_FILE_PATH = "/users/myos/guinew"
GUI_NEW_FOLDER_PATH = "/users/myos/guidir"
GUI_EDITOR_FIXTURE_PATH = "/system/core/resources/gui-16k.txt"
GUI_EDITOR_FIXTURE_PAYLOAD = b"0123456789abcdef" * 1024
GUI_EDITOR_FIXTURE_LENGTH = len(GUI_EDITOR_FIXTURE_PAYLOAD)
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

    def require_small_framebuffer_transition(self, before, after, label):
        changed = sum(left != right for left, right in zip(before, after))
        if len(before) != len(after) or changed < 24:
            raise RegressionFailure(f"{self.name}: {label} did not produce the expected content transition")

    def require_pixel_transition(self, before, after, x, y, label):
        if self.ppm_pixel(before, x, y) == self.ppm_pixel(after, x, y):
            raise RegressionFailure(f"{self.name}: {label} did not change the expected framebuffer pixel")

    def require_region_transition(self, before, after, x, y, width, height, label):
        for row in range(y, y + height):
            for column in range(x, x + width):
                if self.ppm_pixel(before, column, row) != self.ppm_pixel(after, column, row):
                    return
        raise RegressionFailure(f"{self.name}: {label} did not change the expected framebuffer region")

    def require_nonuniform_region(self, image, x, y, width, height, label):
        first = self.ppm_pixel(image, x, y)
        for row in range(y, y + height):
            for column in range(x, x + width):
                if self.ppm_pixel(image, column, row) != first:
                    return
        raise RegressionFailure(f"{self.name}: {label} did not render visible text")

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

    def qmp_key_event(self, qcode, down):
        event = {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}
        self._qmp_request("input-send-event", {"events": [event]})
        time.sleep(0.03)

    def qmp_hotkey(self, modifier, key):
        self.qmp_key_event(modifier, True)
        self.qmp_key_event(key, True)
        self.qmp_key_event(key, False)
        self.qmp_key_event(modifier, False)
        time.sleep(0.10)

    def qmp_press(self, key):
        self.qmp_key_event(key, True)
        self.qmp_key_event(key, False)
        time.sleep(0.10)

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
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        launcher = self.qmp_screendump("hotkey-editor-launcher")
        self.qmp_move(delta_x=115)
        self.qmp_left_click()
        time.sleep(0.25)
        editor = self.qmp_screendump("hotkey-editor-open")
        self.require_framebuffer_transition(launcher, editor, "launcher EDIT NOTE click")
        self.send(b"!\x13")
        time.sleep(0.25)
        self.qmp_hotkey("ctrl", "q")
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

    def gui_save_large_note_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=115)
        self.qmp_left_click()
        time.sleep(0.35)
        editor = self.qmp_screendump("large-note-editor")
        self.send(b"\x13")
        time.sleep(2.00)
        viewer = self.qmp_screendump("large-note-viewer")
        self.require_region_transition(editor, viewer, 330, 205, 160, 20, "16 KiB GUI editor save-to-viewer")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_open_and_exit(self, command=None):
        start = len(self.output)
        self.send((command or f"startgui {NOTE_PATH}") + "\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_modifier_hotkeys_and_exit(self):
        start = len(self.output)
        self.send(f"startgui {NOTE_PATH}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        viewer = self.qmp_screendump("hotkeys-viewer")
        self.qmp_hotkey("alt", "tab")
        focused = self.qmp_screendump("hotkeys-alt-tab")
        self.require_pixel_transition(viewer, focused, 640, 96, "Alt+Tab MONITOR focus")
        self.qmp_hotkey("alt", "f4")
        monitor_closed = self.qmp_screendump("hotkeys-alt-f4-monitor-closed")
        self.require_framebuffer_transition(focused, monitor_closed, "Alt+F4 focused MONITOR close")
        self.qmp_press("esc")
        launcher = self.qmp_screendump("hotkeys-esc-home")
        self.require_framebuffer_transition(monitor_closed, launcher, "Esc viewer return")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_alt_f4_editor_close_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=115)
        self.qmp_left_click()
        time.sleep(0.25)
        editor = self.qmp_screendump("alt-f4-editor-open")
        self.qmp_hotkey("alt", "f4")
        time.sleep(0.35)
        viewer = self.qmp_screendump("alt-f4-editor-viewer")
        self.require_region_transition(editor, viewer, 332, 210, 96, 12, "Alt+F4 editor close to viewer")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_installed_app_tile_and_exit(self, app_count):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        launcher = self.qmp_screendump("app-tile-launcher")
        if app_count == 1:
            self.qmp_move(delta_y=-96)
        elif app_count == 4:
            self.qmp_move(delta_x=-228, delta_y=-96)
        else:
            raise RegressionFailure(f"{self.name}: unsupported app-tile regression count {app_count}")
        self.qmp_left_click()
        time.sleep(0.25)
        console = self.qmp_screendump("app-tile-console")
        self.require_framebuffer_transition(launcher, console, "installed app tile launch")
        self.expect("editor", start)
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_live_clock_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        before = self.qmp_screendump("live-clock-before")
        self.require_nonuniform_region(before, 1168, 12, 56, 7, "desktop clock widget")
        time.sleep(1.75)
        after = self.qmp_screendump("live-clock-after")
        self.require_region_transition(before, after, 1168, 12, 56, 7, "live desktop clock")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_mouse_notes_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=-80)
        before = self.qmp_screendump("desktop-before-click")
        self.require_nonuniform_region(before, 1168, 12, 56, 7, "desktop clock widget")
        self.require_nonuniform_region(before, 1156, 764, 100, 7, "desktop task status")
        self.qmp_left_click()
        time.sleep(0.25)
        after = self.qmp_screendump("desktop-after-click")
        self.require_framebuffer_transition(before, after, "launcher NOTES click")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_launcher_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        launcher = self.qmp_screendump("files-launcher")
        # The fourth centered launcher tile (FILES) occupies x=800..935;
        # startgui begins the pointer at screen center (640, 400).
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-browser")
        self.require_framebuffer_transition(launcher, browser, "launcher FILES click")
        self.require_nonuniform_region(browser, 334, 210, 66, 7, "FILES current-path title")
        self.require_nonuniform_region(browser, 441, 284, 28, 7, "FILES byte-size metadata")
        # Start at the FILES tile click position (870, 400). The parent row is
        # in the NOTES browser content area at approximately (500, 262).
        self.qmp_move(delta_x=-370, delta_y=138)
        time.sleep(0.10)
        parent_ready = self.qmp_screendump("files-parent-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        root_browser = self.qmp_screendump("files-root-browser")
        self.require_region_transition(parent_ready, root_browser, 330, 245, 200, 84, "FILES parent navigation")
        self.require_region_transition(parent_ready, root_browser, 334, 210, 66, 7, "FILES parent path title")
        # From the parent-row click position (501, 262), the first root entry
        # is the /system directory at browser row three (about y=289).
        self.qmp_move(delta_y=-27)
        time.sleep(0.10)
        system_ready = self.qmp_screendump("files-system-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        system_browser = self.qmp_screendump("files-system-browser")
        self.require_region_transition(system_ready, system_browser, 330, 245, 200, 84, "FILES /system directory navigation")
        self.require_region_transition(system_ready, system_browser, 334, 210, 66, 7, "FILES /system path title")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_create_empty_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # The fourth centered launcher tile is FILES at x=800..935; the
        # initial pointer is screen center. The browser NEW FILE row is row
        # eight at approximately (500, 350) in the NOTES content window.
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-create-browser")
        self.qmp_move(delta_x=-370, delta_y=50)
        time.sleep(0.10)
        create_ready = self.qmp_screendump("files-create-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        prompt = self.qmp_screendump("files-create-prompt")
        self.require_small_framebuffer_transition(browser, prompt, "FILES NEW FILE prompt")
        self.require_region_transition(create_ready, prompt, 330, 245, 200, 36, "FILES NEW FILE action")
        for key in ("g", "u", "i", "n", "e", "w"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        editor = self.qmp_screendump("files-create-editor")
        self.require_small_framebuffer_transition(prompt, editor, "FILES new empty-file editor")
        self.require_region_transition(prompt, editor, 330, 210, 200, 48, "FILES new empty-file editor action")
        self.qmp_hotkey("ctrl", "s")
        time.sleep(0.25)
        self.qmp_press("esc")
        time.sleep(0.15)
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_create_folder_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # NEW FOLDER is directly below the established NEW FILE row: launch
        # FILES, then click the ninth browser action row at roughly (500, 362).
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-folder-browser")
        self.qmp_move(delta_x=-370, delta_y=38)
        time.sleep(0.10)
        create_ready = self.qmp_screendump("files-folder-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        prompt = self.qmp_screendump("files-folder-prompt")
        self.require_small_framebuffer_transition(browser, prompt, "FILES NEW FOLDER prompt")
        self.require_region_transition(create_ready, prompt, 330, 245, 200, 36, "FILES NEW FOLDER action")
        for key in ("g", "u", "i", "d", "i", "r"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        created = self.qmp_screendump("files-folder-created")
        self.require_region_transition(prompt, created, 330, 210, 200, 60, "FILES new folder creation")
        self.qmp_hotkey("ctrl", "q")
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
        self.require_region_transition(editor, viewer, 332, 210, 96, 12, "editor close to viewer")
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


def require_system_inventory(name, output, firmware):
    required = (
        b"MYOS SYSTEM INVENTORY",
        b"[boot]",
        b"myos_version=0.13.1-gui-preview.1",
        b"architecture=x86_64",
        b"firmware=" + firmware,
        b"initramfs=ready",
        b"[drivers]",
        b"compiled_in=1",
        b"[devices]",
        b"driver=framebuffer",
    )
    for expected in required:
        if expected not in output:
            decoded = output[-4096:].decode("utf-8", errors="replace")
            raise RegressionFailure(f"{name}: system inventory lacks {expected!r}\n{decoded}")


def run_bios(image_path, work_dir):
    guest = Guest("bios", image_path, work_dir)
    try:
        guest.expect("[ok] Firmware: BIOS")
        guest.expect("[ok] Persistent storage mount: ready")
        guest.expect(PROMPT)
        inventory_start = len(guest.output)
        guest.command("sysinfo", "MYOS SYSTEM INVENTORY")
        require_system_inventory("BIOS", bytes(guest.output[inventory_start:]), b"BIOS")
        inventory_list_start = len(guest.output)
        guest.command("ls /system/live", "[dir] processes")
        inventory_list_output = bytes(guest.output[inventory_list_start:])
        if b"[dir] boot" not in inventory_list_output or b"[dir] drivers" not in inventory_list_output:
            raise RegressionFailure(f"BIOS: live inventory directories are missing\n{guest._tail()}")
        guest.command("write /system/live/boot/info blocked", "Unable to write file.")
        guest.command("ls /system/core", "[dir] apps")
        guest.command("help tree", "run tree remains a compatibility form.")
        tree_start = len(guest.output)
        guest.command("tree /system", "tree:")
        tree_output = bytes(guest.output[tree_start:])
        for expected in (b"[D] core", b"[D] apps", b"[F] tree.elf", b"tree: "):
            if expected not in tree_output:
                raise RegressionFailure(f"BIOS: direct tree output lacks {expected!r}\\n{guest._tail()}")
        guest.command("run tree /system", "tree:")
        guest.command("help find", "run find remains a compatibility form.")
        find_start = len(guest.output)
        guest.command("find TrEe /system/core", "find:")
        find_output = bytes(guest.output[find_start:])
        for expected in (b"[F] /system/core/apps/tree.elf", b"1 match(es)"):
            if expected not in find_output:
                raise RegressionFailure(f"BIOS: direct find output lacks {expected!r}\\n{guest._tail()}")
        guest.command("run find TrEe /system/core", "find:")
        guest.command("help head", "run head remains a compatibility form.")
        head_start = len(guest.output)
        guest.command("head /system/core/resources/motd.txt 2", "Welcome to MyOS.")
        head_output = bytes(guest.output[head_start:])
        if b"Welcome to MyOS.\nThe initramfs VFS is mounted read-only.\n" not in head_output.replace(b"\r", b""):
            raise RegressionFailure(f"BIOS: direct head output lacks first two MOTD lines\n{guest._tail()}")
        guest.command("run head /system/core/resources/motd.txt 2", "Welcome to MyOS.")
        guest.command("help stat", "run stat remains a compatibility form.")
        stat_start = len(guest.output)
        guest.command("stat /system/core/resources/motd.txt", "type: regular")
        stat_output = bytes(guest.output[stat_start:])
        if b"size: 124 bytes" not in stat_output:
            raise RegressionFailure(f"BIOS: direct stat output lacks MOTD size\n{guest._tail()}")
        guest.command("run stat /system/core/resources/motd.txt", "type: regular")
        guest.command("help tail", "run tail remains a compatibility form.")
        tail_start = len(guest.output)
        guest.command("tail /system/core/resources/motd.txt 2", "The initramfs VFS is mounted read-only.")
        tail_output = bytes(guest.output[tail_start:]).replace(b"\r", b"")
        if b"The initramfs VFS is mounted read-only.\nUse ls to inspect bundled files and cat <file> to read text files.\n" not in tail_output:
            raise RegressionFailure(f"BIOS: direct tail output lacks last two MOTD lines\n{guest._tail()}")
        guest.command("run tail /system/core/resources/motd.txt 2", "The initramfs VFS is mounted read-only.")
        guest.command("help sort", "run sort remains a compatibility form.")
        sort_start = len(guest.output)
        guest.command("sort /system/core/resources/motd.txt", "The initramfs VFS is mounted read-only.")
        sort_output = bytes(guest.output[sort_start:]).replace(b"\r", b"")
        if b"The initramfs VFS is mounted read-only.\nUse ls to inspect bundled files and cat <file> to read text files.\nWelcome to MyOS.\n" not in sort_output:
            raise RegressionFailure(f"BIOS: direct sort output is not ASCII ordered\n{guest._tail()}")
        guest.command("run sort /system/core/resources/motd.txt", "The initramfs VFS is mounted read-only.")
        stack_start = len(guest.output)
        guest.command("run stackprobe", "stackprobe:")
        stack_output = bytes(guest.output[stack_start:])
        if b"stackprobe: 12288 bytes checksum 1566720" not in stack_output:
            raise RegressionFailure(f"BIOS: four-page user stack probe failed\\n{guest._tail()}")
        guest.command(f"write {DEFAULT_GUI_NOTE_PATH} base")
        guest.gui_edit_and_exit()
        guest.command(f"cat {DEFAULT_GUI_NOTE_PATH}", "base!")
        guest.command(f"write {NOTE_PATH} base")
        guest.gui_modifier_hotkeys_and_exit()
        guest.gui_alt_f4_editor_close_and_exit()
        guest.gui_live_clock_and_exit()
        guest.gui_mouse_notes_and_exit()
        guest.gui_files_launcher_and_exit()
        guest.gui_files_create_empty_and_exit()
        guest.command(f"stat {GUI_NEW_FILE_PATH}", "0 bytes")
        guest.gui_files_create_folder_and_exit()
        guest.command(f"stat {GUI_NEW_FOLDER_PATH}", "type: directory")
        guest.gui_mouse_window_chrome_and_exit()
        guest.gui_mouse_editor_close_and_exit()
        guest.gui_open_and_exit("startgui home")
        guest.console_edit_and_save(EDITOR_TEXT_PATH, b"first\nsecond\n")
        text_start = len(guest.output)
        guest.command(f"cat {EDITOR_TEXT_PATH}", "first")
        text_output = guest.output[text_start:]
        if b"first\nsecond" not in text_output and b"first\r\nsecond" not in text_output:
            raise RegressionFailure(f"BIOS: editor text readback is not exact\n{guest._tail()}")
        copy_payload = b"copy:" + b"x" * 300
        guest.console_edit_and_save(COPY_SOURCE_PATH, copy_payload)
        guest.command("help cp", "run cp remains a compatibility form.")
        copy_start = len(guest.output)
        guest.command(f"cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "Copied 305 byte(s)")
        copy_output = bytes(guest.output[copy_start:])
        if b"Copied 305 byte(s)" not in copy_output:
            raise RegressionFailure(f"BIOS: direct shell cp did not report a 305-byte copy\n{guest._tail()}")
        copy_read_start = len(guest.output)
        guest.command(f"cat {COPY_TARGET_PATH}", "copy:")
        copy_read_output = bytes(guest.output[copy_read_start:])
        if copy_payload not in copy_read_output:
            raise RegressionFailure(f"BIOS: SDK cp target readback is not exact\n{guest._tail()}")
        guest.command(f"cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "target must not exist")
        guest.command(f"run cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "target must not exist")
        wc_payload = b"x " * 127 + b"edge\n"
        guest.console_edit_and_save(WC_WORD_PATH, wc_payload)
        guest.command("help wc", "run wc remains a compatibility form.")
        guest.command(f"wc {WC_WORD_PATH}", f"1 lines, 128 words, 259 bytes: {WC_WORD_PATH}")
        guest.command(f"run wc {WC_WORD_PATH}", "1 lines, 128 words, 259 bytes")
        grep_payload = b"z" * 126 + b"\n" + b"x" * 122 + b"needle\nneedle-crosses\nno-match\n"
        guest.console_edit_and_save(GREP_MATCH_PATH, grep_payload)
        guest.command("help grep", "run grep remains a compatibility form.")
        grep_start = len(guest.output)
        guest.command(f"grep needle {GREP_MATCH_PATH}", "needle-crosses")
        grep_output = bytes(guest.output[grep_start:]).replace(b"\r", b"")
        if grep_output.count(b"needle-crosses\n") != 1 or b"x" * 122 + b"needle" in grep_output:
            raise RegressionFailure(f"BIOS: direct grep did not skip the overlong matching line or print the short match exactly\n{guest._tail()}")
        guest.command(f"run grep needle {GREP_MATCH_PATH}", "needle-crosses")
        editor_source = b"set 0\njump_if_zero done\nwrite \"bad\\n\"\nlabel done:\nwrite \"editor\\n\"\nexit 44\n"
        guest.console_edit_and_save(EDITOR_SOURCE_PATH, editor_source)
        guest.command(f"build {EDITOR_SOURCE_PATH} {EDITOR_ELF_PATH}", "exited with status 0")
        guest.command(f"install {EDITOR_ELF_PATH} {EDITOR_APP_PATH}", "exited with status 0")
        guest.gui_installed_app_tile_and_exit(1)
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
        variable_source = 'set 73;store 2;set 0;load 2;jump_if 73 matched;write "bad\\n";jump done;label matched:;write "VAR\\n";label done:;exit 48'
        guest.console_edit_and_save(VARIABLE_SOURCE_PATH, variable_source.encode("ascii"))
        guest.command(f"build {VARIABLE_SOURCE_PATH} {VARIABLE_ELF_PATH}", "exited with status 0")
        guest.command(f"install {VARIABLE_ELF_PATH} {VARIABLE_APP_PATH}", "exited with status 0")
        variable_start = len(guest.output)
        guest.command("run release-vars", "exited with status 48")
        variable_output = bytes(guest.output[variable_start:])
        if (b"bad\n" in variable_output or b"bad\r\n" in variable_output
                or (b"VAR\n" not in variable_output and b"VAR\r\n" not in variable_output)):
            raise RegressionFailure(f"BIOS: store/load native variable did not preserve the conditional byte\n{guest._tail()}")
        arithmetic_source = 'set 250;add 8;store 3;set 0;load 3;sub 2;jump_if_zero matched;write "bad\\n";jump done;label matched:;write "ARITH\\n";label done:;exit 49'
        guest.console_edit_and_save(ARITHMETIC_SOURCE_PATH, arithmetic_source.encode("ascii"))
        guest.command(f"build {ARITHMETIC_SOURCE_PATH} {ARITHMETIC_ELF_PATH}", "exited with status 0")
        guest.command(f"install {ARITHMETIC_ELF_PATH} {ARITHMETIC_APP_PATH}", "exited with status 0")
        arithmetic_start = len(guest.output)
        guest.command("run release-arithmetic", "exited with status 49")
        arithmetic_output = bytes(guest.output[arithmetic_start:])
        if (b"bad\n" in arithmetic_output or b"bad\r\n" in arithmetic_output
                or (b"ARITH\n" not in arithmetic_output and b"ARITH\r\n" not in arithmetic_output)):
            raise RegressionFailure(f"BIOS: modular add/sub native arithmetic did not preserve the expected zero branch\n{guest._tail()}")
        muldiv_source = 'set 200;mul 2;add 57;div 3;store 4;set 0;load 4;sub 67;jump_if_zero matched;write "bad\\n";jump done;label matched:;write "MULDIV\\n";label done:;exit 50'
        guest.console_edit_and_save(MULDIV_SOURCE_PATH, muldiv_source.encode("ascii"))
        guest.command(f"build {MULDIV_SOURCE_PATH} {MULDIV_ELF_PATH}", "exited with status 0")
        guest.command(f"install {MULDIV_ELF_PATH} {MULDIV_APP_PATH}", "exited with status 0")
        muldiv_start = len(guest.output)
        guest.command("run release-muldiv", "exited with status 50")
        muldiv_output = bytes(guest.output[muldiv_start:])
        if (b"bad\n" in muldiv_output or b"bad\r\n" in muldiv_output
                or (b"MULDIV\n" not in muldiv_output and b"MULDIV\r\n" not in muldiv_output)):
            raise RegressionFailure(f"BIOS: modular mul/div native arithmetic did not preserve the expected zero branch\n{guest._tail()}")
        bitwise_source = 'set 240;not;and 63;or 128;store 6;set 0;load 6;jump_if 143 matched;write "bad\\n";jump done;label matched:;write "BITWISE\\n";label done:;exit 52'
        guest.console_edit_and_save(BITWISE_SOURCE_PATH, bitwise_source.encode("ascii"))
        guest.command(f"build {BITWISE_SOURCE_PATH} {BITWISE_ELF_PATH}", "exited with status 0")
        guest.command(f"install {BITWISE_ELF_PATH} {BITWISE_APP_PATH}", "exited with status 0")
        bitwise_start = len(guest.output)
        guest.command("run release-bitwise", "exited with status 52")
        bitwise_output = bytes(guest.output[bitwise_start:])
        if (b"bad\n" in bitwise_output or b"bad\r\n" in bitwise_output
                or (b"BITWISE\n" not in bitwise_output and b"BITWISE\r\n" not in bitwise_output)):
            raise RegressionFailure(f"BIOS: bounded native not/and/or did not preserve the expected byte branch\n{guest._tail()}")
        xor_source = 'set 170;xor 255;xor 85;store 7;set 1;load 7;jump_if_zero matched;write "bad\\n";jump done;label matched:;write "XOR\\n";label done:;exit 53'
        guest.console_edit_and_save(XOR_SOURCE_PATH, xor_source.encode("ascii"))
        guest.command(f"build {XOR_SOURCE_PATH} {XOR_ELF_PATH}", "exited with status 0")
        guest.command(f"install {XOR_ELF_PATH} {XOR_APP_PATH}", "exited with status 0")
        xor_start = len(guest.output)
        guest.command("run release-xor", "exited with status 53")
        xor_output = bytes(guest.output[xor_start:])
        if (b"bad\n" in xor_output or b"bad\r\n" in xor_output
                or (b"XOR\n" not in xor_output and b"XOR\r\n" not in xor_output)):
            raise RegressionFailure(f"BIOS: bounded native xor did not preserve the expected zero byte branch\n{guest._tail()}")
        shift_source = 'set 3;shl 5;shr 4;store 1;set 0;load 1;jump_if 6 matched;write "bad\\n";jump done;label matched:;write "SHIFT\\n";label done:;exit 54'
        guest.console_edit_and_save(SHIFT_SOURCE_PATH, shift_source.encode("ascii"))
        guest.command(f"build {SHIFT_SOURCE_PATH} {SHIFT_ELF_PATH}", "exited with status 0")
        guest.command(f"install {SHIFT_ELF_PATH} {SHIFT_APP_PATH}", "exited with status 0")
        shift_start = len(guest.output)
        guest.command("run release-shift", "exited with status 54")
        shift_output = bytes(guest.output[shift_start:])
        if (b"bad\n" in shift_output or b"bad\r\n" in shift_output
                or (b"SHIFT\n" not in shift_output and b"SHIFT\r\n" not in shift_output)):
            raise RegressionFailure(f"BIOS: bounded native shl/shr did not preserve the expected byte branch\n{guest._tail()}")
        cmp_source = 'set 73;store 5;set 73;cmp 5;jump_if_zero equal;write "bad\\n";jump after_equal;label equal:;write "EQ\\n";label after_equal:;set 72;cmp 5;jump_if_nonzero different;write "bad\\n";jump done;label different:;write "NE\\n";label done:;exit 51'
        guest.console_edit_and_save(CMP_SOURCE_PATH, cmp_source.encode("ascii"))
        guest.command(f"build {CMP_SOURCE_PATH} {CMP_ELF_PATH}", "exited with status 0")
        guest.command(f"install {CMP_ELF_PATH} {CMP_APP_PATH}", "exited with status 0")
        cmp_start = len(guest.output)
        guest.command("run release-cmp", "exited with status 51")
        cmp_output = bytes(guest.output[cmp_start:])
        if (b"bad\n" in cmp_output or b"bad\r\n" in cmp_output
                or (b"EQ\n" not in cmp_output and b"EQ\r\n" not in cmp_output)
                or (b"NE\n" not in cmp_output and b"NE\r\n" not in cmp_output)):
            raise RegressionFailure(f"BIOS: native cmp did not preserve equal and non-equal private-slot branches\n{guest._tail()}")
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
        diagnostic = "asm: syntax error; set/load/input must precede"
        guest.command(f"write {BACKWARD_SOURCE_PATH} label start:;write \"x\\n\";jump start;exit 0")
        guest.command(f"build {BACKWARD_SOURCE_PATH} {BACKWARD_ELF_PATH}", diagnostic)
        guest.command(f"write {MISSING_SET_SOURCE_PATH} jump_if 65 done;label done:;exit 0")
        guest.command(f"build {MISSING_SET_SOURCE_PATH} {MISSING_SET_ELF_PATH}", diagnostic)
        guest.command(f"write {CONDITIONAL_BACKWARD_SOURCE_PATH} label start:;set 1;jump_if 1 start;exit 0")
        guest.command(f"build {CONDITIONAL_BACKWARD_SOURCE_PATH} {CONDITIONAL_BACKWARD_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_VARIABLE_SOURCE_PATH} set 1;store 8;exit 0")
        guest.command(f"build {INVALID_VARIABLE_SOURCE_PATH} {INVALID_VARIABLE_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_ARITHMETIC_SOURCE_PATH} set 1;and 256;exit 0")
        guest.command(f"build {INVALID_ARITHMETIC_SOURCE_PATH} {INVALID_ARITHMETIC_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_BITWISE_SOURCE_PATH} not;exit 0")
        guest.command(f"build {INVALID_BITWISE_SOURCE_PATH} {INVALID_BITWISE_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_XOR_SOURCE_PATH} xor 1;exit 0")
        guest.command(f"build {INVALID_XOR_SOURCE_PATH} {INVALID_XOR_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_XOR_BOUND_SOURCE_PATH} set 1;xor 256;exit 0")
        guest.command(f"build {INVALID_XOR_BOUND_SOURCE_PATH} {INVALID_XOR_BOUND_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_SHIFT_SOURCE_PATH} shl 1;exit 0")
        guest.command(f"build {INVALID_SHIFT_SOURCE_PATH} {INVALID_SHIFT_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_SHIFT_SOURCE_PATH} set 1;shl 0;exit 0")
        guest.command(f"build {INVALID_SHIFT_SOURCE_PATH} {INVALID_SHIFT_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_SHIFT_SOURCE_PATH} set 1;shr 8;exit 0")
        guest.command(f"build {INVALID_SHIFT_SOURCE_PATH} {INVALID_SHIFT_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_DIVISION_SOURCE_PATH} set 1;div 0;exit 0")
        guest.command(f"build {INVALID_DIVISION_SOURCE_PATH} {INVALID_DIVISION_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_CMP_SOURCE_PATH} cmp 0;exit 0")
        guest.command(f"build {INVALID_CMP_SOURCE_PATH} {INVALID_CMP_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_CMP_SLOT_SOURCE_PATH} set 1;cmp 8;exit 0")
        guest.command(f"build {INVALID_CMP_SLOT_SOURCE_PATH} {INVALID_CMP_SLOT_ELF_PATH}", diagnostic)
        guest.command(f"install {SDK_WRITE_EXAMPLE_PATH} {SDK_WRITE_APP_PATH}", "exited with status 0")
        sdk_write_start = len(guest.output)
        guest.command(f"run sdk-write {SDK_WRITE_BIOS_TARGET}", "sdk-write: wrote fixed payload")
        sdk_write_output = bytes(guest.output[sdk_write_start:])
        if SDK_WRITE_BIOS_TARGET.encode("ascii") not in sdk_write_output:
            raise RegressionFailure(f"BIOS: sdk-write did not report its target\\n{guest._tail()}")
        sdk_write_read_start = len(guest.output)
        guest.command(f"cat {SDK_WRITE_BIOS_TARGET}", "sdk-write: persistent VFS example")
        if SDK_WRITE_PAYLOAD not in bytes(guest.output[sdk_write_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"BIOS: sdk-write payload readback is not exact\\n{guest._tail()}")
        guest.command(f"run sdk-write {SDK_WRITE_BIOS_TARGET}", "target must not exist")
        guest.command(f"rm {DEFAULT_GUI_NOTE_PATH}", f"Removed {DEFAULT_GUI_NOTE_PATH}")
        guest.command(f"cp {GUI_EDITOR_FIXTURE_PATH} {DEFAULT_GUI_NOTE_PATH}",
                      f"Copied {GUI_EDITOR_FIXTURE_LENGTH} byte(s)")
        guest.gui_save_large_note_and_exit()
        large_gui_start = len(guest.output)
        guest.command(f"cat {DEFAULT_GUI_NOTE_PATH}", "0123456789abcdef")
        if GUI_EDITOR_FIXTURE_PAYLOAD not in guest.output[large_gui_start:]:
            raise RegressionFailure(f"BIOS: 16 KiB GUI editor save/readback is not exact\n{guest._tail()}")
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
        inventory_start = len(guest.output)
        guest.command("sysinfo", "MYOS SYSTEM INVENTORY")
        require_system_inventory("UEFI", bytes(guest.output[inventory_start:]), b"UEFI x86_64")
        guest.command("ls /system/core", "[dir] apps")
        tree_start = len(guest.output)
        guest.command("tree /system/core", "tree:")
        tree_output = bytes(guest.output[tree_start:])
        for expected in (b"[D] apps", b"[F] tree.elf", b"tree:"):
            if expected not in tree_output:
                raise RegressionFailure(f"UEFI: direct tree output lacks {expected!r}\\n{guest._tail()}")
        find_start = len(guest.output)
        guest.command("find find /system/core/apps", "find:")
        find_output = bytes(guest.output[find_start:])
        if b"[F] /system/core/apps/find.elf" not in find_output:
            raise RegressionFailure(f"UEFI: direct find output lacks packaged find.elf\n{guest._tail()}")
        head_start = len(guest.output)
        guest.command("head /system/core/resources/motd.txt 2", "Welcome to MyOS.")
        head_output = bytes(guest.output[head_start:])
        if b"Welcome to MyOS.\nThe initramfs VFS is mounted read-only.\n" not in head_output.replace(b"\r", b""):
            raise RegressionFailure(f"UEFI: direct head output lacks first two MOTD lines\n{guest._tail()}")
        stat_start = len(guest.output)
        guest.command("stat /system/core/resources/motd.txt", "type: regular")
        stat_output = bytes(guest.output[stat_start:])
        if b"size: 124 bytes" not in stat_output:
            raise RegressionFailure(f"UEFI: direct stat output lacks MOTD size\n{guest._tail()}")
        tail_start = len(guest.output)
        guest.command("tail /system/core/resources/motd.txt 2", "The initramfs VFS is mounted read-only.")
        tail_output = bytes(guest.output[tail_start:])
        if b"The initramfs VFS is mounted read-only.\nUse ls to inspect bundled files and cat <file> to read text files.\n" not in tail_output.replace(b"\r", b""):
            raise RegressionFailure(f"UEFI: direct tail output lacks last two MOTD lines\n{guest._tail()}")
        sort_start = len(guest.output)
        guest.command("sort /system/core/resources/motd.txt", "The initramfs VFS is mounted read-only.")
        sort_output = bytes(guest.output[sort_start:]).replace(b"\r", b"")
        if b"The initramfs VFS is mounted read-only.\nUse ls to inspect bundled files and cat <file> to read text files.\nWelcome to MyOS.\n" not in sort_output:
            raise RegressionFailure(f"UEFI: direct sort output is not ASCII ordered\n{guest._tail()}")
        stack_start = len(guest.output)
        guest.command("run stackprobe", "stackprobe:")
        stack_output = bytes(guest.output[stack_start:])
        if b"stackprobe: 12288 bytes checksum 1566720" not in stack_output:
            raise RegressionFailure(f"UEFI: four-page user stack probe failed\\n{guest._tail()}")
        guest.command("write /system/live/boot/info blocked", "Unable to write file.")
        uefi_large_gui_start = len(guest.output)
        guest.command(f"cat {DEFAULT_GUI_NOTE_PATH}", "0123456789abcdef")
        if GUI_EDITOR_FIXTURE_PAYLOAD not in guest.output[uefi_large_gui_start:]:
            raise RegressionFailure(f"UEFI: persisted 16 KiB GUI editor payload is not exact\n{guest._tail()}")
        guest.command(f"write {DEFAULT_GUI_NOTE_PATH} base")
        guest.command(f"cat {NOTE_PATH}", "base")
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
        guest.command(f"cp {COPY_SOURCE_PATH} {COPY_TARGET_PATH}", "target must not exist")
        guest.command(f"wc {WC_WORD_PATH}", f"1 lines, 128 words, 259 bytes: {WC_WORD_PATH}")
        grep_start = len(guest.output)
        guest.command(f"grep needle {GREP_MATCH_PATH}", "needle-crosses")
        grep_output = bytes(guest.output[grep_start:]).replace(b"\r", b"")
        if grep_output.count(b"needle-crosses\n") != 1 or b"x" * 122 + b"needle" in grep_output:
            raise RegressionFailure(f"UEFI: persisted direct grep did not skip the overlong matching line or print the short match exactly\n{guest._tail()}")
        sdk_write_bios_read_start = len(guest.output)
        guest.command(f"cat {SDK_WRITE_BIOS_TARGET}", "sdk-write: persistent VFS example")
        if SDK_WRITE_PAYLOAD not in bytes(guest.output[sdk_write_bios_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"UEFI: persisted BIOS sdk-write payload is not exact\\n{guest._tail()}")
        sdk_write_uefi_start = len(guest.output)
        guest.command(f"run sdk-write {SDK_WRITE_UEFI_TARGET}", "sdk-write: wrote fixed payload")
        sdk_write_uefi_output = bytes(guest.output[sdk_write_uefi_start:])
        if SDK_WRITE_UEFI_TARGET.encode("ascii") not in sdk_write_uefi_output:
            raise RegressionFailure(f"UEFI: persisted sdk-write package did not report its target\\n{guest._tail()}")
        sdk_write_uefi_read_start = len(guest.output)
        guest.command(f"cat {SDK_WRITE_UEFI_TARGET}", "sdk-write: persistent VFS example")
        if SDK_WRITE_PAYLOAD not in bytes(guest.output[sdk_write_uefi_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"UEFI: sdk-write UEFI payload readback is not exact\\n{guest._tail()}")
        run_start = len(guest.output)
        guest.command("run editor-harness", "editor")
        run_output = guest.output[run_start:]
        if b"bad" in run_output or b"exited with status 44" not in run_output:
            raise RegressionFailure(f"UEFI: persisted editor-authored program did not skip code or return status 44\n{guest._tail()}")
        guest.gui_installed_app_tile_and_exit(4)
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
        variable_start = len(guest.output)
        guest.command("run release-vars", "exited with status 48")
        variable_output = bytes(guest.output[variable_start:])
        if (b"bad\n" in variable_output or b"bad\r\n" in variable_output
                or (b"VAR\n" not in variable_output and b"VAR\r\n" not in variable_output)):
            raise RegressionFailure(f"UEFI: persisted store/load native variable did not preserve the conditional byte\n{guest._tail()}")
        arithmetic_start = len(guest.output)
        guest.command("run release-arithmetic", "exited with status 49")
        arithmetic_output = bytes(guest.output[arithmetic_start:])
        if (b"bad\n" in arithmetic_output or b"bad\r\n" in arithmetic_output
                or (b"ARITH\n" not in arithmetic_output and b"ARITH\r\n" not in arithmetic_output)):
            raise RegressionFailure(f"UEFI: persisted modular add/sub native arithmetic did not preserve the expected zero branch\n{guest._tail()}")
        muldiv_start = len(guest.output)
        guest.command("run release-muldiv", "exited with status 50")
        muldiv_output = bytes(guest.output[muldiv_start:])
        if (b"bad\n" in muldiv_output or b"bad\r\n" in muldiv_output
                or (b"MULDIV\n" not in muldiv_output and b"MULDIV\r\n" not in muldiv_output)):
            raise RegressionFailure(f"UEFI: persisted modular mul/div native arithmetic did not preserve the expected zero branch\n{guest._tail()}")
        bitwise_start = len(guest.output)
        guest.command("run release-bitwise", "exited with status 52")
        bitwise_output = bytes(guest.output[bitwise_start:])
        if (b"bad\n" in bitwise_output or b"bad\r\n" in bitwise_output
                or (b"BITWISE\n" not in bitwise_output and b"BITWISE\r\n" not in bitwise_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native not/and/or did not preserve the expected byte branch\n{guest._tail()}")
        xor_start = len(guest.output)
        guest.command("run release-xor", "exited with status 53")
        xor_output = bytes(guest.output[xor_start:])
        if (b"bad\n" in xor_output or b"bad\r\n" in xor_output
                or (b"XOR\n" not in xor_output and b"XOR\r\n" not in xor_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native xor did not preserve the expected zero byte branch\n{guest._tail()}")
        shift_start = len(guest.output)
        guest.command("run release-shift", "exited with status 54")
        shift_output = bytes(guest.output[shift_start:])
        if (b"bad\n" in shift_output or b"bad\r\n" in shift_output
                or (b"SHIFT\n" not in shift_output and b"SHIFT\r\n" not in shift_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native shl/shr did not preserve the expected byte branch\n{guest._tail()}")
        cmp_start = len(guest.output)
        guest.command("run release-cmp", "exited with status 51")
        cmp_output = bytes(guest.output[cmp_start:])
        if (b"bad\n" in cmp_output or b"bad\r\n" in cmp_output
                or (b"EQ\n" not in cmp_output and b"EQ\r\n" not in cmp_output)
                or (b"NE\n" not in cmp_output and b"NE\r\n" not in cmp_output)):
            raise RegressionFailure(f"UEFI: persisted native cmp did not preserve equal and non-equal private-slot branches\n{guest._tail()}")
        args_output_start = len(guest.output)
        guest.command("run release-args ovmf args", "exited with status 47")
        args_output = bytes(guest.output[args_output_start:])
        require_native_line("UEFI persisted native args", args_output, b"[ovmf args]")
        require_time_line("UEFI persisted native args", args_output)
        guest.gui_open_and_exit()
        guest.gui_modifier_hotkeys_and_exit()
        guest.gui_alt_f4_editor_close_and_exit()
        guest.gui_live_clock_and_exit()
        guest.gui_mouse_notes_and_exit()
        guest.gui_files_launcher_and_exit()
        guest.command(f"stat {GUI_NEW_FILE_PATH}", "0 bytes")
        guest.command(f"stat {GUI_NEW_FOLDER_PATH}", "type: directory")
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
