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
NEWPROJ_NAME = "scaffold-harness"
NEWPROJ_DIRECTORY = "/users/myos/projects/scaffold-harness"
NEWPROJ_SOURCE_PATH = "/users/myos/projects/scaffold-harness/main.mya"
NEWPROJ_TEMPLATE = b"# MyOS project template\nwrite \"Hello from MyOS project\\n\"\nexit 0\n"
ARGS_PROJ_NAME = "starter-args"
ARGS_PROJ_DIRECTORY = "/users/myos/projects/starter-args"
ARGS_PROJ_SOURCE_PATH = "/users/myos/projects/starter-args/main.mya"
ARGS_PROJ_TEMPLATE = b"# MyOS argument project template\nwrite \"[\"\nargs\nwrite \"]\\n\"\nexit 0\n"
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
ROTATE_SOURCE_PATH = "/temp/release-rotate.mya"
ROTATE_ELF_PATH = "/users/myos/projects/release-rotate.elf"
ROTATE_APP_PATH = "/apps/release-rotate/main.elf"
INVALID_ROTATE_SOURCE_PATH = "/temp/release-rotate-invalid.mya"
INVALID_ROTATE_ELF_PATH = "/users/myos/projects/release-rotate-invalid.elf"
MOD_SOURCE_PATH = "/temp/release-mod.mya"
MOD_ELF_PATH = "/users/myos/projects/release-mod.elf"
MOD_APP_PATH = "/apps/release-mod/main.elf"
INVALID_MOD_SOURCE_PATH = "/temp/release-mod-invalid.mya"
INVALID_MOD_ELF_PATH = "/users/myos/projects/release-mod-invalid.elf"
NEG_SOURCE_PATH = "/temp/release-neg.mya"
NEG_ELF_PATH = "/users/myos/projects/release-neg.elf"
NEG_APP_PATH = "/apps/release-neg/main.elf"
INVALID_NEG_SOURCE_PATH = "/temp/release-neg-invalid.mya"
INVALID_NEG_ELF_PATH = "/users/myos/projects/release-neg-invalid.elf"
INC_SOURCE_PATH = "/temp/release-inc.mya"
INC_ELF_PATH = "/users/myos/projects/release-inc.elf"
INC_APP_PATH = "/apps/release-inc/main.elf"
INVALID_INC_SOURCE_PATH = "/temp/release-inc-invalid.mya"
INVALID_INC_ELF_PATH = "/users/myos/projects/release-inc-invalid.elf"
DEC_SOURCE_PATH = "/temp/release-dec.mya"
DEC_ELF_PATH = "/users/myos/projects/release-dec.elf"
DEC_APP_PATH = "/apps/release-dec/main.elf"
INVALID_DEC_SOURCE_PATH = "/temp/release-dec-invalid.mya"
INVALID_DEC_ELF_PATH = "/users/myos/projects/release-dec-invalid.elf"
SWAP_SOURCE_PATH = "/temp/release-swap.mya"
SWAP_ELF_PATH = "/users/myos/projects/release-swap.elf"
SWAP_APP_PATH = "/apps/release-swap/main.elf"
INVALID_SWAP_SOURCE_PATH = "/temp/release-swap-invalid.mya"
INVALID_SWAP_ELF_PATH = "/users/myos/projects/release-swap-invalid.elf"
TEST_SOURCE_PATH = "/temp/release-test.mya"
TEST_ELF_PATH = "/users/myos/projects/release-test.elf"
TEST_APP_PATH = "/apps/release-test/main.elf"
INVALID_TEST_SOURCE_PATH = "/temp/release-test-invalid.mya"
INVALID_TEST_ELF_PATH = "/users/myos/projects/release-test-invalid.elf"
PARITY_SOURCE_PATH = "/temp/release-parity.mya"
PARITY_ELF_PATH = "/users/myos/projects/release-parity.elf"
PARITY_APP_PATH = "/apps/release-parity/main.elf"
INVALID_PARITY_SOURCE_PATH = "/temp/release-parity-invalid.mya"
INVALID_PARITY_ELF_PATH = "/users/myos/projects/release-parity-invalid.elf"
CLZ_SOURCE_PATH = "/temp/release-clz.mya"
CLZ_ELF_PATH = "/users/myos/projects/release-clz.elf"
CLZ_APP_PATH = "/apps/release-clz/main.elf"
INVALID_CLZ_SOURCE_PATH = "/temp/release-clz-invalid.mya"
INVALID_CLZ_ELF_PATH = "/users/myos/projects/release-clz-invalid.elf"
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
GUI_COPY_SOURCE_PATH = "/users/myos/guicopysource"
GUI_COPY_FILE_PATH = "/users/myos/guicopytarget"
GUI_RENAME_TARGET_PATH = "/users/myos/guirenamed"
GUI_MOVE_TARGET_PATH = "/users/myos/projects/guirenamed"
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
        self.require_region_transition(viewer, focused, 1098, 764, 56, 7, "Alt+Tab footer focus indicator")
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

    def gui_installed_app_tile_and_exit(self, app_name, expected_output):
        list_start = len(self.output)
        self.command("ls /apps", f"[dir] {app_name}")
        names = []
        for line in bytes(self.output[list_start:]).replace(b"\r", b"").splitlines():
            if line.startswith(b"[dir] "):
                name = line[6:].decode("ascii")
                if len(name) <= 15:
                    names.append(name)
        if app_name not in names:
            raise RegressionFailure(f"{self.name}: launcher-visible app {app_name} was not listed\n{self._tail()}")
        app_index = names.index(app_name)
        visible_count = min(len(names), 4)
        if app_index >= visible_count:
            raise RegressionFailure(f"{self.name}: installed app {app_name} is outside the four-tile launcher bound\n{self._tail()}")
        total_width = visible_count * 136 + (visible_count - 1) * 16
        tile_center_x = (1280 - total_width) // 2 + app_index * (136 + 16) + 68
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        launcher = self.qmp_screendump("app-tile-launcher")
        self.qmp_move(delta_x=tile_center_x - 640, delta_y=-96)
        self.qmp_left_click()
        time.sleep(0.25)
        console = self.qmp_screendump("app-tile-console")
        self.require_framebuffer_transition(launcher, console, "installed app tile launch")
        self.expect(expected_output, start)
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

    def gui_project_workspace_and_exit(self):
        start = len(self.output)
        self.send("startgui projects\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        browser = self.qmp_screendump("project-workspace-browser")
        self.require_nonuniform_region(browser, 334, 210, 66, 7, "project workspace current-path title")
        self.require_nonuniform_region(browser, 441, 284, 28, 7, "project workspace entry metadata")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_workspace_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name}\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        browser = self.qmp_screendump("direct-project-workspace-browser")
        self.require_nonuniform_region(browser, 334, 210, 66, 7, "direct project workspace current-path title")
        self.require_nonuniform_region(browser, 441, 284, 28, 7, "direct project workspace source metadata")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_build_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} build\n")
        self.expect("Started process ", start)
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_run_and_exit(self, project_name, arguments, expected_output):
        start = len(self.output)
        command = f"startgui project {project_name} run"
        if arguments:
            command += f" {arguments}"
        self.send(command + "\n")
        self.expect("Started process ", start)
        self.expect(expected_output, start)
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_install_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} install\n")
        self.expect("Started process ", start)
        self.expect("Installed ", start)
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_install_output_missing_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} install\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        if b"exited with status" in bytes(self.output[start:]):
            raise RegressionFailure(f"GUI install missing-output request unexpectedly spawned a child\n{self._tail()}")
        status = self.qmp_screendump("direct-project-install-output-missing-status")
        self.require_nonuniform_region(status, 330, 210, 200, 24, "direct project install missing-output status")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_run_output_missing_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} run\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        if b"exited with status" in bytes(self.output[start:]):
            raise RegressionFailure(f"GUI run missing-output request unexpectedly spawned a child\n{self._tail()}")
        status = self.qmp_screendump("direct-project-run-output-missing-status")
        self.require_nonuniform_region(status, 330, 210, 200, 24, "direct project run missing-output status")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_build_source_missing_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} build\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        if b"exited with status" in bytes(self.output[start:]):
            raise RegressionFailure(f"GUI build missing-source request unexpectedly spawned a child\n{self._tail()}")
        status = self.qmp_screendump("direct-project-build-source-missing-status")
        self.require_nonuniform_region(status, 330, 210, 200, 24, "direct project build missing-source status")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_status_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} status\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        status = self.qmp_screendump("direct-project-lifecycle-status")
        self.require_nonuniform_region(status, 334, 210, 66, 7, "direct project status title")
        self.require_nonuniform_region(status, 330, 245, 200, 60, "direct project status rows")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_direct_project_editor_and_exit(self, project_name):
        start = len(self.output)
        self.send(f"startgui project {project_name} edit\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        editor = self.qmp_screendump("direct-project-source-editor")
        self.require_nonuniform_region(editor, 334, 210, 66, 7, "direct project source editor title")
        self.require_nonuniform_region(editor, 330, 245, 200, 36, "direct project source editor content")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_invalid_project_workspace_and_exit(self):
        start = len(self.output)
        self.send("startgui project no-such-project edit\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        status = self.qmp_screendump("invalid-project-workspace-status")
        self.require_nonuniform_region(status, 330, 210, 200, 24, "invalid project workspace status")
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

    def gui_files_copy_multichunk_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # COPY follows DELETE. The initial pointer is centered, so its browser
        # row is 12 pixels above the DELETE test hit position.
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-copy-browser")
        self.qmp_move(delta_x=-370, delta_y=14)
        time.sleep(0.10)
        copy_ready = self.qmp_screendump("files-copy-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        source_prompt = self.qmp_screendump("files-copy-source-prompt")
        self.require_small_framebuffer_transition(browser, source_prompt, "FILES COPY source prompt")
        self.require_region_transition(copy_ready, source_prompt, 330, 245, 200, 36, "FILES COPY action")
        for key in ("g", "u", "i", "c", "o", "p", "y", "s", "o", "u", "r", "c", "e"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        target_prompt = self.qmp_screendump("files-copy-target-prompt")
        self.require_region_transition(source_prompt, target_prompt, 330, 210, 200, 60, "FILES COPY target prompt")
        for key in ("g", "u", "i", "c", "o", "p", "y", "t", "a", "r", "g", "e", "t"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        copied = self.qmp_screendump("files-copy-refresh")
        self.require_region_transition(target_prompt, copied, 330, 210, 200, 84, "FILES copy refresh")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_copy_existing_target_rejected_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # Reuse the same source and existing target used by the successful
        # multi-chunk copy. The second action must show a failure rather than
        # replacing the persisted target.
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-copy-reject-browser")
        self.qmp_move(delta_x=-370, delta_y=14)
        time.sleep(0.10)
        copy_ready = self.qmp_screendump("files-copy-reject-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        source_prompt = self.qmp_screendump("files-copy-reject-source-prompt")
        self.require_small_framebuffer_transition(browser, source_prompt, "FILES COPY rejection source prompt")
        self.require_region_transition(copy_ready, source_prompt, 330, 245, 200, 36, "FILES COPY rejection action")
        for key in ("g", "u", "i", "c", "o", "p", "y", "s", "o", "u", "r", "c", "e"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        target_prompt = self.qmp_screendump("files-copy-reject-target-prompt")
        self.require_region_transition(source_prompt, target_prompt, 330, 210, 200, 60, "FILES COPY rejection target prompt")
        for key in ("g", "u", "i", "c", "o", "p", "y", "t", "a", "r", "g", "e", "t"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        rejected = self.qmp_screendump("files-copy-reject-status")
        self.require_region_transition(target_prompt, rejected, 330, 210, 200, 72, "FILES copy no-overwrite rejection")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_rename_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-rename-browser")
        # RENAME is directly below COPY in the fixed File Workspace action area.
        # This mouse path verifies the framebuffer row-to-action mapping as well
        # as the ring-3 two-step rename prompt.
        self.qmp_move(delta_x=-370, delta_y=2)
        time.sleep(0.10)
        rename_ready = self.qmp_screendump("files-rename-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        source_prompt = self.qmp_screendump("files-rename-source-prompt")
        self.require_small_framebuffer_transition(browser, source_prompt, "FILES RENAME source prompt")
        self.require_region_transition(rename_ready, source_prompt, 330, 245, 200, 36, "FILES RENAME action")
        self.send("guicopytarget\n")
        time.sleep(0.25)
        target_prompt = self.qmp_screendump("files-rename-target-prompt")
        self.require_region_transition(source_prompt, target_prompt, 330, 210, 200, 60, "FILES RENAME target prompt")
        self.send("guirenamed\n")
        time.sleep(0.25)
        renamed = self.qmp_screendump("files-rename-refresh")
        self.require_region_transition(target_prompt, renamed, 330, 210, 200, 84, "FILES rename refresh")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_move_existing_target_rejected_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-move-reject-browser")
        self.qmp_move(delta_x=-370, delta_y=-10)
        time.sleep(0.10)
        move_ready = self.qmp_screendump("files-move-reject-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        source_prompt = self.qmp_screendump("files-move-reject-source-prompt")
        self.require_small_framebuffer_transition(browser, source_prompt, "FILES MOVE rejection source prompt")
        self.require_region_transition(move_ready, source_prompt, 330, 245, 200, 36, "FILES MOVE rejection action")
        self.send("guirenamed\n")
        time.sleep(0.25)
        target_prompt = self.qmp_screendump("files-move-reject-target-prompt")
        self.require_region_transition(source_prompt, target_prompt, 330, 210, 200, 60, "FILES MOVE rejection target prompt")
        self.send("/users/myos/projects\n")
        time.sleep(0.25)
        rejected = self.qmp_screendump("files-move-reject-status")
        self.require_region_transition(target_prompt, rejected, 330, 210, 200, 84, "FILES MOVE no-overwrite rejection")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_move_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-move-browser")
        # MOVE is directly below RENAME. This path verifies the framebuffer
        # pointer mapping, source prompt, absolute destination prompt and VFS move.
        self.qmp_move(delta_x=-370, delta_y=-10)
        time.sleep(0.10)
        move_ready = self.qmp_screendump("files-move-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        source_prompt = self.qmp_screendump("files-move-source-prompt")
        self.require_small_framebuffer_transition(browser, source_prompt, "FILES MOVE source prompt")
        self.require_region_transition(move_ready, source_prompt, 330, 245, 200, 36, "FILES MOVE action")
        self.send("guirenamed\n")
        time.sleep(0.25)
        target_prompt = self.qmp_screendump("files-move-target-prompt")
        self.require_region_transition(source_prompt, target_prompt, 330, 210, 200, 60, "FILES MOVE target prompt")
        self.send("/users/myos/projects\n")
        time.sleep(0.25)
        moved = self.qmp_screendump("files-move-refresh")
        self.require_region_transition(target_prompt, moved, 330, 210, 200, 84, "FILES MOVE refresh")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_search_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # RENAME now occupies the row below COPY, so SEARCH is two rows below COPY.
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-search-browser")
        self.qmp_move(delta_x=-370, delta_y=38)
        time.sleep(0.10)
        search_ready = self.qmp_screendump("files-search-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        prompt = self.qmp_screendump("files-search-prompt")
        self.require_small_framebuffer_transition(browser, prompt, "FILES SEARCH prompt")
        self.require_region_transition(search_ready, prompt, 330, 245, 200, 36, "FILES SEARCH action")
        for key in ("g", "u", "i", "d", "i", "r"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        results = self.qmp_screendump("files-search-results")
        self.require_region_transition(prompt, results, 330, 210, 200, 96, "FILES SEARCH results")
        # The first matching entry occupies the fixed browser result row (three rows above SEARCH).
        self.qmp_move(delta_y=-60)
        self.qmp_left_click()
        time.sleep(0.25)
        opened = self.qmp_screendump("files-search-opened-directory")
        self.require_small_framebuffer_transition(results, opened, "FILES SEARCH open result")
        self.qmp_hotkey("ctrl", "q")
        self.expect("exited with status 0", start)
        self.expect(PROMPT, start)

    def gui_files_delete_empty_and_exit(self):
        start = len(self.output)
        self.send("startgui\n")
        self.expect("Started process ", start)
        time.sleep(0.25)
        # DELETE is directly below NEW FOLDER. It removes the zero-byte
        # guinew fixture created by the earlier File Workspace workflow only
        # after its named confirmation prompt receives a second Enter.
        self.qmp_move(delta_x=229)
        self.qmp_left_click()
        time.sleep(0.25)
        browser = self.qmp_screendump("files-delete-browser")
        self.qmp_move(delta_x=-370, delta_y=26)
        time.sleep(0.10)
        delete_ready = self.qmp_screendump("files-delete-ready")
        self.qmp_left_click()
        time.sleep(0.25)
        prompt = self.qmp_screendump("files-delete-prompt")
        self.require_small_framebuffer_transition(browser, prompt, "FILES DELETE prompt")
        self.require_region_transition(delete_ready, prompt, 330, 245, 200, 36, "FILES DELETE action")
        for key in ("g", "u", "i", "n", "e", "w"):
            self.qmp_press(key)
        self.qmp_press("ret")
        time.sleep(0.25)
        confirmation = self.qmp_screendump("files-delete-confirmation")
        self.require_region_transition(prompt, confirmation, 330, 210, 200, 72, "FILES DELETE confirmation")
        self.qmp_press("ret")
        time.sleep(0.25)
        deleted = self.qmp_screendump("files-delete-refresh")
        self.require_region_transition(confirmation, deleted, 330, 210, 200, 72, "FILES delete refresh")
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
        guest.command(f"cp {GUI_EDITOR_FIXTURE_PATH} {GUI_COPY_SOURCE_PATH}", "Copied 16384 byte(s)")
        guest.gui_files_copy_multichunk_and_exit()
        guest.command(f"stat {GUI_COPY_SOURCE_PATH}", "16384 bytes")
        guest.command(f"stat {GUI_COPY_FILE_PATH}", "16384 bytes")
        guest.gui_files_copy_existing_target_rejected_and_exit()
        guest.command(f"stat {GUI_COPY_FILE_PATH}", "16384 bytes")
        guest.gui_files_rename_and_exit()
        guest.command(f"stat {GUI_COPY_FILE_PATH}", "stat: path not found")
        guest.command(f"stat {GUI_RENAME_TARGET_PATH}", "16384 bytes")
        guest.command(f"cp {GUI_RENAME_TARGET_PATH} {GUI_MOVE_TARGET_PATH}", "Copied 16384 byte(s)")
        guest.gui_files_move_existing_target_rejected_and_exit()
        guest.command(f"stat {GUI_RENAME_TARGET_PATH}", "16384 bytes")
        guest.command(f"stat {GUI_MOVE_TARGET_PATH}", "16384 bytes")
        guest.command(f"rm {GUI_MOVE_TARGET_PATH}", f"Removed {GUI_MOVE_TARGET_PATH}")
        guest.gui_files_move_and_exit()
        guest.command(f"stat {GUI_RENAME_TARGET_PATH}", "stat: path not found")
        guest.command(f"stat {GUI_MOVE_TARGET_PATH}", "16384 bytes")
        guest.gui_project_workspace_and_exit()
        guest.gui_files_search_and_exit()
        guest.gui_files_delete_empty_and_exit()
        guest.command(f"stat {GUI_NEW_FILE_PATH}", "stat: path not found")
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
        guest.command("help newproj", "hello is the default; args writes the bounded native argument string")
        guest.command("newproj rejected-template nope", "Usage: newproj <project-name> [hello|args]")
        guest.command("newproj rejected-template hello", "Created project /users/myos/projects/rejected-template")
        guest.command("rm /users/myos/projects/rejected-template/main.mya", "Removed")
        guest.command("rm /users/myos/projects/rejected-template", "Removed")
        guest.command("newproj gui-build-no-source", "Created project /users/myos/projects/gui-build-no-source")
        guest.command("rm /users/myos/projects/gui-build-no-source/main.mya", "Removed")
        guest.gui_direct_project_build_source_missing_and_exit("gui-build-no-source")
        guest.command("rm /users/myos/projects/gui-build-no-source", "Removed")
        guest.command(f"newproj {ARGS_PROJ_NAME} args", f"Created project {ARGS_PROJ_DIRECTORY}")
        args_template_read_start = len(guest.output)
        guest.command(f"cat {ARGS_PROJ_SOURCE_PATH}", "write \"[\"")
        if ARGS_PROJ_TEMPLATE not in bytes(guest.output[args_template_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"BIOS: args newproj template is not exact\n{guest._tail()}")
        guest.command(f"buildproj {ARGS_PROJ_NAME}", "exited with status 0")
        guest.gui_direct_project_run_and_exit(ARGS_PROJ_NAME, "starter args", "[starter args]")
        args_template_run_start = len(guest.output)
        guest.command(f"runproj {ARGS_PROJ_NAME} starter args", "exited with status 0")
        args_template_run_output = bytes(guest.output[args_template_run_start:])
        if b"[starter args]\n" not in args_template_run_output and b"[starter args]\r\n" not in args_template_run_output:
            raise RegressionFailure(f"BIOS: args newproj template did not forward direct arguments\n{guest._tail()}")
        guest.command(f"installproj {ARGS_PROJ_NAME}", "exited with status 0")
        args_package_run_start = len(guest.output)
        guest.command(f"run {ARGS_PROJ_NAME} starter args", "exited with status 0")
        if b"[starter args]\n" not in bytes(guest.output[args_package_run_start:]) and b"[starter args]\r\n" not in bytes(guest.output[args_package_run_start:]):
            raise RegressionFailure(f"BIOS: installed args starter did not forward package arguments\n{guest._tail()}")
        guest.command(f"rmproj {ARGS_PROJ_NAME}", "Build output is present. Run cleanproj first.")
        guest.command(f"cleanproj {ARGS_PROJ_NAME}", "Removed build output")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "source: READY")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "package: READY")
        guest.command(f"newproj {NEWPROJ_NAME}", f"Created project {NEWPROJ_DIRECTORY}")
        newproj_read_start = len(guest.output)
        guest.command(f"cat {NEWPROJ_SOURCE_PATH}", "Hello from MyOS project")
        newproj_source_output = bytes(guest.output[newproj_read_start:]).replace(b"\r", b"")
        if NEWPROJ_TEMPLATE not in newproj_source_output:
            raise RegressionFailure(f"BIOS: newproj template is not exact\n{guest._tail()}")
        guest.command(f"newproj {NEWPROJ_NAME}", "Unable to create project.")
        duplicate_read_start = len(guest.output)
        guest.command(f"cat {NEWPROJ_SOURCE_PATH}", "Hello from MyOS project")
        if NEWPROJ_TEMPLATE not in bytes(guest.output[duplicate_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"BIOS: duplicate newproj changed its existing template\n{guest._tail()}")
        guest.command("help editproj", "Opens /users/myos/projects/<project-name>/main.mya in the bounded editor.")
        guest.command("help projlist", "Lists up to 128 valid project directories with read-only source, build and installed package status rows.")
        guest.command("help projstatus", "Shows regular-file state and size for fixed source, build and installed package paths.")
        guest.command("help uninstallproj", "Removes only the regular installed /apps/<project-name>/main.elf; project source and build stay unchanged.")
        guest.command("help cleanproj", "Removes only the regular generated <project>/main.elf; source and installed package stay unchanged.")
        guest.command("help rmproj", "After cleanproj, removes only the regular project source and empty project directory; installed package stays unchanged.")
        guest.command(f"projstatus {NEWPROJ_NAME}", "source: READY")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: MISSING")
        guest.gui_direct_project_run_output_missing_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_install_output_missing_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_workspace_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_status_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_editor_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_build_and_exit(NEWPROJ_NAME)
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: READY")
        guest.gui_direct_project_run_and_exit(NEWPROJ_NAME, "", "Hello from MyOS project")
        guest.gui_direct_project_install_and_exit(NEWPROJ_NAME)
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        guest.command(f"cleanproj {NEWPROJ_NAME}", "Removed build output")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: MISSING")
        guest.gui_invalid_project_workspace_and_exit()
        project_list_start = len(guest.output)
        guest.command("projlist", f"PROJECT {NEWPROJ_NAME}")
        project_list_output = bytes(guest.output[project_list_start:])
        if f"PROJECT {ARGS_PROJ_NAME}".encode("ascii") not in project_list_output or b"  source: READY" not in project_list_output:
            raise RegressionFailure(f"BIOS: projlist did not report both bounded starter projects\n{guest._tail()}")
        editproj_start = len(guest.output)
        guest.send(f"editproj {NEWPROJ_NAME}\n")
        guest.expect(f"File: {NEWPROJ_SOURCE_PATH}", editproj_start)
        guest.send(b"\x11")
        guest.expect("exited with status 0", editproj_start)
        guest.expect(PROMPT, editproj_start)
        guest.command("help buildproj", "Builds /users/myos/projects/<project-name>/main.mya to main.elf.")
        guest.command("help runproj", "Runs only the regular generated /users/myos/projects/<project-name>/main.elf without installation.")
        guest.command("help installproj", "an existing package target is replaced")
        guest.command(f"buildproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"runproj {NEWPROJ_NAME} " + "x" * 128, "arguments: at most 127 bytes")
        direct_run_start = len(guest.output)
        guest.command(f"runproj {NEWPROJ_NAME}", "exited with status 0")
        direct_run_output = bytes(guest.output[direct_run_start:])
        if b"Hello from MyOS project\n" not in direct_run_output and b"Hello from MyOS project\r\n" not in direct_run_output:
            raise RegressionFailure(f"BIOS: runproj did not run the uninstalled generated template\n{guest._tail()}")
        guest.command(f"run {NEWPROJ_DIRECTORY}/not-main.elf", "Unable to start program.")
        guest.command(f"installproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"installproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: READY")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        newproj_run_start = len(guest.output)
        guest.command(f"run {NEWPROJ_NAME}", "exited with status 0")
        newproj_run_output = bytes(guest.output[newproj_run_start:])
        if b"Hello from MyOS project\n" not in newproj_run_output and b"Hello from MyOS project\r\n" not in newproj_run_output:
            raise RegressionFailure(f"BIOS: newproj package did not run its fixed template\n{guest._tail()}")
        guest.command(f"uninstallproj {NEWPROJ_NAME}", "Removed package output")
        guest.command(f"projstatus {NEWPROJ_NAME}", "source: READY")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: READY")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: MISSING")
        guest.command(f"uninstallproj {NEWPROJ_NAME}", "Package output is already absent.")
        guest.command(f"run {NEWPROJ_NAME}", "Unable to start program.")
        guest.command(f"runproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"installproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        guest.command(f"cleanproj {NEWPROJ_NAME}", "Removed build output")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        guest.command(f"runproj {NEWPROJ_NAME}", "Build output is missing. Run buildproj first.")
        clean_run_start = len(guest.output)
        guest.command(f"run {NEWPROJ_NAME}", "exited with status 0")
        clean_run_output = bytes(guest.output[clean_run_start:])
        if b"Hello from MyOS project\n" not in clean_run_output and b"Hello from MyOS project\r\n" not in clean_run_output:
            raise RegressionFailure(f"BIOS: cleanproj changed the installed package\n{guest._tail()}")
        guest.command(f"buildproj {NEWPROJ_NAME}", "exited with status 0")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: READY")
        rebuilt_direct_run_start = len(guest.output)
        guest.command(f"runproj {NEWPROJ_NAME}", "exited with status 0")
        rebuilt_direct_run_output = bytes(guest.output[rebuilt_direct_run_start:])
        if b"Hello from MyOS project\n" not in rebuilt_direct_run_output and b"Hello from MyOS project\r\n" not in rebuilt_direct_run_output:
            raise RegressionFailure(f"BIOS: rebuilt runproj output changed the generated template\n{guest._tail()}")
        guest.command(f"cleanproj {NEWPROJ_NAME}", "Removed build output")
        guest.command(f"rmproj {ARGS_PROJ_NAME}", "Removed project directory")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "source: MISSING")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "package: READY")
        guest.command(f"rmproj {ARGS_PROJ_NAME}", "Project is already absent.")
        project_list_start = len(guest.output)
        guest.command("projlist", f"PROJECT {NEWPROJ_NAME}")
        project_list_output = bytes(guest.output[project_list_start:])
        if f"PROJECT {ARGS_PROJ_NAME}".encode("ascii") in project_list_output:
            raise RegressionFailure(f"BIOS: rmproj left the removed project in projlist\n{guest._tail()}")
        args_package_run_start = len(guest.output)
        guest.command(f"run {ARGS_PROJ_NAME} starter args", "exited with status 0")
        if b"[starter args]\n" not in bytes(guest.output[args_package_run_start:]) and b"[starter args]\r\n" not in bytes(guest.output[args_package_run_start:]):
            raise RegressionFailure(f"BIOS: rmproj changed the installed args package\n{guest._tail()}")
        editor_source = b"set 0\njump_if_zero done\nwrite \"bad\\n\"\nlabel done:\nwrite \"editor\\n\"\nexit 44\n"
        guest.console_edit_and_save(EDITOR_SOURCE_PATH, editor_source)
        guest.command(f"build {EDITOR_SOURCE_PATH} {EDITOR_ELF_PATH}", "exited with status 0")
        guest.command(f"install {EDITOR_ELF_PATH} {EDITOR_APP_PATH}", "exited with status 0")
        guest.gui_installed_app_tile_and_exit("editor-harness", "editor")
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
        rotate_source = 'set 129;rol 1;ror 2;store 2;set 0;load 2;jump_if 192 matched;write "bad\\n";jump done;label matched:;write "ROTATE\\n";label done:;exit 55'
        guest.console_edit_and_save(ROTATE_SOURCE_PATH, rotate_source.encode("ascii"))
        guest.command(f"build {ROTATE_SOURCE_PATH} {ROTATE_ELF_PATH}", "exited with status 0")
        guest.command(f"install {ROTATE_ELF_PATH} {ROTATE_APP_PATH}", "exited with status 0")
        rotate_start = len(guest.output)
        guest.command("run release-rotate", "exited with status 55")
        rotate_output = bytes(guest.output[rotate_start:])
        if (b"bad\n" in rotate_output or b"bad\r\n" in rotate_output
                or (b"ROTATE\n" not in rotate_output and b"ROTATE\r\n" not in rotate_output)):
            raise RegressionFailure(f"BIOS: bounded native rol/ror did not preserve the expected byte branch\n{guest._tail()}")
        mod_source = 'set 200;mod 57;store 2;set 0;load 2;jump_if 29 matched;write "bad\\n";jump done;label matched:;write "MOD\\n";label done:;exit 56'
        guest.console_edit_and_save(MOD_SOURCE_PATH, mod_source.encode("ascii"))
        guest.command(f"build {MOD_SOURCE_PATH} {MOD_ELF_PATH}", "exited with status 0")
        guest.command(f"install {MOD_ELF_PATH} {MOD_APP_PATH}", "exited with status 0")
        mod_start = len(guest.output)
        guest.command("run release-mod", "exited with status 56")
        mod_output = bytes(guest.output[mod_start:])
        if (b"bad\n" in mod_output or b"bad\r\n" in mod_output
                or (b"MOD\n" not in mod_output and b"MOD\r\n" not in mod_output)):
            raise RegressionFailure(f"BIOS: bounded native mod did not preserve the expected byte remainder branch\n{guest._tail()}")
        neg_source = 'set 7;neg;store 2;set 0;load 2;jump_if 249 matched;write "bad\\n";jump done;label matched:;write "NEG\\n";label done:;exit 57'
        guest.console_edit_and_save(NEG_SOURCE_PATH, neg_source.encode("ascii"))
        guest.command(f"build {NEG_SOURCE_PATH} {NEG_ELF_PATH}", "exited with status 0")
        guest.command(f"install {NEG_ELF_PATH} {NEG_APP_PATH}", "exited with status 0")
        neg_start = len(guest.output)
        guest.command("run release-neg", "exited with status 57")
        neg_output = bytes(guest.output[neg_start:])
        if (b"bad\n" in neg_output or b"bad\r\n" in neg_output
                or (b"NEG\n" not in neg_output and b"NEG\r\n" not in neg_output)):
            raise RegressionFailure(f"BIOS: bounded native neg did not preserve the expected byte branch\n{guest._tail()}")
        inc_source = 'set 255;inc;store 1;set 3;load 1;jump_if_zero wrapped;write "bad\\n";jump done;label wrapped:;write "INC\\n";label done:;exit 58'
        guest.console_edit_and_save(INC_SOURCE_PATH, inc_source.encode("ascii"))
        guest.command(f"build {INC_SOURCE_PATH} {INC_ELF_PATH}", "exited with status 0")
        guest.command(f"install {INC_ELF_PATH} {INC_APP_PATH}", "exited with status 0")
        inc_start = len(guest.output)
        guest.command("run release-inc", "exited with status 58")
        inc_output = bytes(guest.output[inc_start:])
        if (b"bad\n" in inc_output or b"bad\r\n" in inc_output
                or (b"INC\n" not in inc_output and b"INC\r\n" not in inc_output)):
            raise RegressionFailure(f"BIOS: bounded native inc did not preserve the expected wrapping byte branch\n{guest._tail()}")
        dec_source = 'set 0;dec;store 1;set 3;load 1;jump_if 255 wrapped;write "bad\\n";jump done;label wrapped:;write "DEC\\n";label done:;exit 59'
        guest.console_edit_and_save(DEC_SOURCE_PATH, dec_source.encode("ascii"))
        guest.command(f"build {DEC_SOURCE_PATH} {DEC_ELF_PATH}", "exited with status 0")
        guest.command(f"install {DEC_ELF_PATH} {DEC_APP_PATH}", "exited with status 0")
        dec_start = len(guest.output)
        guest.command("run release-dec", "exited with status 59")
        dec_output = bytes(guest.output[dec_start:])
        if (b"bad\n" in dec_output or b"bad\r\n" in dec_output
                or (b"DEC\n" not in dec_output and b"DEC\r\n" not in dec_output)):
            raise RegressionFailure(f"BIOS: bounded native dec did not preserve the expected wrapping byte branch\n{guest._tail()}")
        swap_source = 'set 73;store 4;set 12;swap 4;jump_if 73 swapped;write "bad\\n";jump done;label swapped:;write "SWAP\\n";label done:;exit 60'
        guest.console_edit_and_save(SWAP_SOURCE_PATH, swap_source.encode("ascii"))
        guest.command(f"build {SWAP_SOURCE_PATH} {SWAP_ELF_PATH}", "exited with status 0")
        guest.command(f"install {SWAP_ELF_PATH} {SWAP_APP_PATH}", "exited with status 0")
        swap_start = len(guest.output)
        guest.command("run release-swap", "exited with status 60")
        swap_output = bytes(guest.output[swap_start:])
        if (b"bad\n" in swap_output or b"bad\r\n" in swap_output
                or (b"SWAP\n" not in swap_output and b"SWAP\r\n" not in swap_output)):
            raise RegressionFailure(f"BIOS: bounded native swap did not preserve the expected byte exchange branch\n{guest._tail()}")
        clz_source = 'set 32;clz;jump_if 2 nonzero;write "bad\\n";jump done;label nonzero:;set 0;clz;jump_if 8 zero;write "bad\\n";jump done;label zero:;write "CLZ\\n";label done:;exit 63'
        guest.console_edit_and_save(CLZ_SOURCE_PATH, clz_source.encode("ascii"))
        guest.command(f"build {CLZ_SOURCE_PATH} {CLZ_ELF_PATH}", "exited with status 0")
        guest.command(f"install {CLZ_ELF_PATH} {CLZ_APP_PATH}", "exited with status 0")
        clz_start = len(guest.output)
        guest.command("run release-clz", "exited with status 63")
        clz_output = bytes(guest.output[clz_start:])
        if (b"bad\n" in clz_output or b"bad\r\n" in clz_output
                or (b"CLZ\n" not in clz_output and b"CLZ\r\n" not in clz_output)):
            raise RegressionFailure(f"BIOS: bounded native clz did not preserve expected leading-zero branches\n{guest._tail()}")
        parity_source = 'set 3;parity;jump_if_nonzero even;write "bad\\n";jump done;label even:;set 1;parity;jump_if_zero odd;write "bad\\n";jump done;label odd:;write "PARITY\\n";label done:;exit 62'
        guest.console_edit_and_save(PARITY_SOURCE_PATH, parity_source.encode("ascii"))
        guest.command(f"build {PARITY_SOURCE_PATH} {PARITY_ELF_PATH}", "exited with status 0")
        guest.command(f"install {PARITY_ELF_PATH} {PARITY_APP_PATH}", "exited with status 0")
        parity_start = len(guest.output)
        guest.command("run release-parity", "exited with status 62")
        parity_output = bytes(guest.output[parity_start:])
        if (b"bad\n" in parity_output or b"bad\r\n" in parity_output
                or (b"PARITY\n" not in parity_output and b"PARITY\r\n" not in parity_output)):
            raise RegressionFailure(f"BIOS: bounded native parity did not preserve expected even and odd byte branches\n{guest._tail()}")
        test_source = 'set 160;test 128;jump_if_nonzero matched;write "bad\\n";jump done;label matched:;set 160;test 15;jump_if_zero clear;write "bad\\n";jump done;label clear:;write "TEST\\n";label done:;exit 61'
        guest.console_edit_and_save(TEST_SOURCE_PATH, test_source.encode("ascii"))
        guest.command(f"build {TEST_SOURCE_PATH} {TEST_ELF_PATH}", "exited with status 0")
        guest.command(f"install {TEST_ELF_PATH} {TEST_APP_PATH}", "exited with status 0")
        test_start = len(guest.output)
        guest.command("run release-test", "exited with status 61")
        test_output = bytes(guest.output[test_start:])
        if (b"bad\n" in test_output or b"bad\r\n" in test_output
                or (b"TEST\n" not in test_output and b"TEST\r\n" not in test_output)):
            raise RegressionFailure(f"BIOS: bounded native test did not preserve the expected byte predicate branches\n{guest._tail()}")
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
        guest.command(f"write {INVALID_ROTATE_SOURCE_PATH} rol 1;exit 0")
        guest.command(f"build {INVALID_ROTATE_SOURCE_PATH} {INVALID_ROTATE_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_ROTATE_SOURCE_PATH} set 1;rol 0;exit 0")
        guest.command(f"build {INVALID_ROTATE_SOURCE_PATH} {INVALID_ROTATE_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_ROTATE_SOURCE_PATH} set 1;ror 8;exit 0")
        guest.command(f"build {INVALID_ROTATE_SOURCE_PATH} {INVALID_ROTATE_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_DIVISION_SOURCE_PATH} set 1;div 0;exit 0")
        guest.command(f"build {INVALID_DIVISION_SOURCE_PATH} {INVALID_DIVISION_ELF_PATH}", diagnostic)
        for temporary_source in (BACKWARD_SOURCE_PATH, MISSING_SET_SOURCE_PATH, CONDITIONAL_BACKWARD_SOURCE_PATH,
                                 INVALID_VARIABLE_SOURCE_PATH, INVALID_ARITHMETIC_SOURCE_PATH,
                                 INVALID_BITWISE_SOURCE_PATH, INVALID_XOR_SOURCE_PATH,
                                 INVALID_XOR_BOUND_SOURCE_PATH, INVALID_SHIFT_SOURCE_PATH,
                                 INVALID_ROTATE_SOURCE_PATH, INVALID_DIVISION_SOURCE_PATH):
            guest.command(f"rm {temporary_source}")
        guest.command(f"write {INVALID_MOD_SOURCE_PATH} mod 1;exit 0")
        guest.command(f"build {INVALID_MOD_SOURCE_PATH} {INVALID_MOD_ELF_PATH}", diagnostic)
        guest.command(f"write {INVALID_MOD_SOURCE_PATH} set 1;mod 0;exit 0")
        guest.command(f"build {INVALID_MOD_SOURCE_PATH} {INVALID_MOD_ELF_PATH}", diagnostic)
        for temporary_source in (BACKWARD_SOURCE_PATH, MISSING_SET_SOURCE_PATH, CONDITIONAL_BACKWARD_SOURCE_PATH,
                                 INVALID_VARIABLE_SOURCE_PATH, INVALID_ARITHMETIC_SOURCE_PATH,
                                 INVALID_BITWISE_SOURCE_PATH, INVALID_XOR_SOURCE_PATH,
                                 INVALID_XOR_BOUND_SOURCE_PATH, INVALID_SHIFT_SOURCE_PATH,
                                 INVALID_ROTATE_SOURCE_PATH, INVALID_DIVISION_SOURCE_PATH,
                                 INVALID_MOD_SOURCE_PATH):
            guest.command(f"rm {temporary_source}")
        guest.command(f"write {INVALID_NEG_SOURCE_PATH} neg;exit 0")
        guest.command(f"build {INVALID_NEG_SOURCE_PATH} {INVALID_NEG_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_NEG_SOURCE_PATH}")
        guest.command(f"write {INVALID_INC_SOURCE_PATH} inc;exit 0")
        guest.command(f"build {INVALID_INC_SOURCE_PATH} {INVALID_INC_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_INC_SOURCE_PATH}")
        guest.command(f"write {INVALID_DEC_SOURCE_PATH} dec;exit 0")
        guest.command(f"build {INVALID_DEC_SOURCE_PATH} {INVALID_DEC_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_DEC_SOURCE_PATH}")
        guest.command(f"write {INVALID_SWAP_SOURCE_PATH} swap 0;exit 0")
        guest.command(f"build {INVALID_SWAP_SOURCE_PATH} {INVALID_SWAP_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_SWAP_SOURCE_PATH}")
        guest.command(f"write {INVALID_TEST_SOURCE_PATH} test 1;exit 0")
        guest.command(f"build {INVALID_TEST_SOURCE_PATH} {INVALID_TEST_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_TEST_SOURCE_PATH}")
        guest.command(f"write {INVALID_PARITY_SOURCE_PATH} parity;exit 0")
        guest.command(f"build {INVALID_PARITY_SOURCE_PATH} {INVALID_PARITY_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_PARITY_SOURCE_PATH}")
        guest.command(f"write {INVALID_CLZ_SOURCE_PATH} clz;exit 0")
        guest.command(f"build {INVALID_CLZ_SOURCE_PATH} {INVALID_CLZ_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_CLZ_SOURCE_PATH}")
        guest.command(f"write {INVALID_CMP_SOURCE_PATH} cmp 0;exit 0")
        guest.command(f"build {INVALID_CMP_SOURCE_PATH} {INVALID_CMP_ELF_PATH}", diagnostic)
        guest.command(f"rm {INVALID_CMP_SOURCE_PATH}")
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
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "source: MISSING")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {ARGS_PROJ_NAME}", "package: READY")
        args_package_run_start = len(guest.output)
        guest.command(f"run {ARGS_PROJ_NAME} ovmf args", "exited with status 0")
        if b"[ovmf args]\n" not in bytes(guest.output[args_package_run_start:]) and b"[ovmf args]\r\n" not in bytes(guest.output[args_package_run_start:]):
            raise RegressionFailure(f"UEFI: rmproj did not preserve the installed args package\n{guest._tail()}")
        project_list_start = len(guest.output)
        guest.command("projlist", f"PROJECT {NEWPROJ_NAME}")
        project_list_output = bytes(guest.output[project_list_start:])
        if f"PROJECT {ARGS_PROJ_NAME}".encode("ascii") in project_list_output:
            raise RegressionFailure(f"UEFI: projlist retained the removed args project\n{guest._tail()}")
        newproj_read_start = len(guest.output)
        guest.command(f"cat {NEWPROJ_SOURCE_PATH}", "Hello from MyOS project")
        guest.command(f"projstatus {NEWPROJ_NAME}", "source: READY")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: MISSING")
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        guest.command(f"runproj {NEWPROJ_NAME}", "Build output is missing. Run buildproj first.")
        if NEWPROJ_TEMPLATE not in bytes(guest.output[newproj_read_start:]).replace(b"\r", b""):
            raise RegressionFailure(f"UEFI: persisted newproj template is not exact\\n{guest._tail()}")
        guest.gui_direct_project_workspace_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_status_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_editor_and_exit(NEWPROJ_NAME)
        guest.gui_direct_project_build_and_exit(NEWPROJ_NAME)
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: READY")
        guest.gui_direct_project_run_and_exit(NEWPROJ_NAME, "", "Hello from MyOS project")
        guest.gui_direct_project_install_and_exit(NEWPROJ_NAME)
        guest.command(f"projstatus {NEWPROJ_NAME}", "package: READY")
        guest.command(f"cleanproj {NEWPROJ_NAME}", "Removed build output")
        guest.command(f"projstatus {NEWPROJ_NAME}", "build: MISSING")
        newproj_run_start = len(guest.output)
        guest.command(f"run {NEWPROJ_NAME}", "exited with status 0")
        newproj_run_output = bytes(guest.output[newproj_run_start:])
        if b"Hello from MyOS project\n" not in newproj_run_output and b"Hello from MyOS project\r\n" not in newproj_run_output:
            raise RegressionFailure(f"UEFI: persisted newproj package did not run its fixed template\\n{guest._tail()}")
        run_start = len(guest.output)
        guest.command("run editor-harness", "editor")
        run_output = guest.output[run_start:]
        if b"bad" in run_output or b"exited with status 44" not in run_output:
            raise RegressionFailure(f"UEFI: persisted editor-authored program did not skip code or return status 44\n{guest._tail()}")
        guest.gui_installed_app_tile_and_exit("editor-harness", "editor")
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
        rotate_start = len(guest.output)
        guest.command("run release-rotate", "exited with status 55")
        rotate_output = bytes(guest.output[rotate_start:])
        if (b"bad\n" in rotate_output or b"bad\r\n" in rotate_output
                or (b"ROTATE\n" not in rotate_output and b"ROTATE\r\n" not in rotate_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native rol/ror did not preserve the expected byte branch\n{guest._tail()}")
        mod_start = len(guest.output)
        guest.command("run release-mod", "exited with status 56")
        mod_output = bytes(guest.output[mod_start:])
        if (b"bad\n" in mod_output or b"bad\r\n" in mod_output
                or (b"MOD\n" not in mod_output and b"MOD\r\n" not in mod_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native mod did not preserve the expected byte remainder branch\n{guest._tail()}")
        neg_start = len(guest.output)
        guest.command("run release-neg", "exited with status 57")
        neg_output = bytes(guest.output[neg_start:])
        if (b"bad\n" in neg_output or b"bad\r\n" in neg_output
                or (b"NEG\n" not in neg_output and b"NEG\r\n" not in neg_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native neg did not preserve the expected byte branch\n{guest._tail()}")
        inc_start = len(guest.output)
        guest.command("run release-inc", "exited with status 58")
        inc_output = bytes(guest.output[inc_start:])
        if (b"bad\n" in inc_output or b"bad\r\n" in inc_output
                or (b"INC\n" not in inc_output and b"INC\r\n" not in inc_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native inc did not preserve the expected wrapping byte branch\n{guest._tail()}")
        dec_start = len(guest.output)
        guest.command("run release-dec", "exited with status 59")
        dec_output = bytes(guest.output[dec_start:])
        if (b"bad\n" in dec_output or b"bad\r\n" in dec_output
                or (b"DEC\n" not in dec_output and b"DEC\r\n" not in dec_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native dec did not preserve the expected wrapping byte branch\n{guest._tail()}")
        swap_start = len(guest.output)
        guest.command("run release-swap", "exited with status 60")
        swap_output = bytes(guest.output[swap_start:])
        if (b"bad\n" in swap_output or b"bad\r\n" in swap_output
                or (b"SWAP\n" not in swap_output and b"SWAP\r\n" not in swap_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native swap did not preserve the expected byte exchange branch\n{guest._tail()}")
        clz_start = len(guest.output)
        guest.command("run release-clz", "exited with status 63")
        clz_output = bytes(guest.output[clz_start:])
        if (b"bad\n" in clz_output or b"bad\r\n" in clz_output
                or (b"CLZ\n" not in clz_output and b"CLZ\r\n" not in clz_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native clz did not preserve expected leading-zero branches\n{guest._tail()}")
        parity_start = len(guest.output)
        guest.command("run release-parity", "exited with status 62")
        parity_output = bytes(guest.output[parity_start:])
        if (b"bad\n" in parity_output or b"bad\r\n" in parity_output
                or (b"PARITY\n" not in parity_output and b"PARITY\r\n" not in parity_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native parity did not preserve expected even and odd byte branches\n{guest._tail()}")
        test_start = len(guest.output)
        guest.command("run release-test", "exited with status 61")
        test_output = bytes(guest.output[test_start:])
        if (b"bad\n" in test_output or b"bad\r\n" in test_output
                or (b"TEST\n" not in test_output and b"TEST\r\n" not in test_output)):
            raise RegressionFailure(f"UEFI: persisted bounded native test did not preserve the expected byte predicate branches\n{guest._tail()}")
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
        guest.command(f"stat {GUI_NEW_FILE_PATH}", "stat: path not found")
        guest.command(f"stat {GUI_COPY_SOURCE_PATH}", "16384 bytes")
        guest.command(f"stat {GUI_COPY_FILE_PATH}", "stat: path not found")
        guest.command(f"stat {GUI_RENAME_TARGET_PATH}", "stat: path not found")
        guest.command(f"stat {GUI_MOVE_TARGET_PATH}", "16384 bytes")
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
