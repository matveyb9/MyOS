# GUI bring-up: native framebuffer desktop

<p align="center">
  <a href="GUI_BRINGUP_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


This document describes the experimental GUI integrated in the QEMU-validated `main` line. It is not part of the stable console baseline `v0.12.1-console`; `console-stable` remains unchanged, while `feature/gui` is retained only as historical pre-integration evidence. The GUI remains a native x86_64 component of MyOS: it is drawn directly into the RGB framebuffer, without a web runtime, external graphical toolkit or dynamic memory allocation.

## Launch

Build the current `main` raw image. For testing persistent storage, attach the image to QEMU as an IDE disk.

```bash
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

After kernel bootstrap MyOS automatically launches the user shell after a three-second countdown. Press `K` during the countdown if you need the diagnostic kernel shell; in that case `init` is left as a manual way to start the user shell. Then start the MyOS desktop. Without an argument `startgui` opens the bounded **MYOS DESKTOP** launcher; an optional absolute-file argument opens that file in the viewer.

```text
startgui
# Compatibility alias: startgui home
# File viewer: startgui /users/myos/files/notes/note
# Project root: startgui projects
# Exact project workspace: startgui project <name>
# Exact project source editor: startgui project <name> edit
# Read-only project lifecycle view: startgui project <name> status
# Fixed project build: startgui project <name> build
# Fixed project run: startgui project <name> run [arguments]
# Fixed project install: startgui project <name> install
# Fixed project package removal: startgui project <name> uninstall
# Fixed project build cleanup: startgui project <name> clean
# Fixed project starter creation: startgui project <name> new [hello|args]
# Fixed clean project workspace removal: startgui project <name> remove
```

`startgui` is an ordinary ring-3 program. Without arguments it opens the bounded **MYOS DESKTOP** home view; `startgui home` remains a compatibility alias. Its mouse-first launcher exposes clickable `SYSTEM`, `NOTES`, `EDIT NOTE` and `FILES` tiles, plus up to four discovered installed-app tiles and a top-bar `X` exit control. In viewer or editor mode the active `NOTES` surface is raised above the static windows; click any exposed window title bar to raise it. Each window also has its own `X`: `SYSTEM` and `MONITOR` are hidden, the `NOTES` viewer returns home, and the `NOTES` editor cancels its unsaved draft and restores the selected-file viewer. The separate top-bar `X` still ends the GUI session. GUI-level keyboard fallback follows standard desktop roles: `Alt+Tab` moves focus to the next visible window, `Alt+F4` closes the focused window using the same state-specific action as its `X`, `Esc` returns the viewer home or cancels the editor draft, and `Ctrl+Q` exits to the same user shell. The launcher and window actions are otherwise mouse-only. The editor retains `Ctrl+S` to save. The top bar includes a compositor-owned `HH:MM:SS` clock widget; the footer shows bounded snapshots in the form `TASKS <allocated> RUN <runnable>`. The clock refreshes once per PIT second by restoring the 11×11 pointer when necessary and repainting only its 72×32 top-bar rectangle; it does not redraw desktop windows, tiles or the footer. The task values refresh when the GUI owner submits new content for redraw. Both use the existing RTC and fixed 16-slot scheduler counters, are display-only and require no new ABI. It creates a restricted GUI session through the existing syscall boundary, reads and edits only bounded user-space payloads, and the kernel receives only validated syscall requests. `startgui <absolute-path>` remains a viewer-first direct launch. The `FILES` tile starts a file browser at `/users/myos/`; `startgui projects` enters the same browser directly at `/users/myos/projects`, while `startgui project <name>` accepts only a 1–31-character ASCII letter/digit/`-`/`_` name, revalidates that exact existing project directory and opens it directly. Its exact `edit` suffix revalidates only `<project>/main.mya` as a regular writable source, then enters the established GUI editor; `Ctrl+Q`, save and cancel retain their existing editor behavior. Its exact `status` suffix is read-only: after the same project-directory revalidation it reports fixed source, build and installed-package `READY <size> bytes`, `MISSING` or `NOT REGULAR` rows through at most 128 directory probes per fixed parent. Its exact `build` suffix revalidates only the regular fixed source then invokes the established assembler with `<project>/main.mya` and `<project>/main.elf` as its sole arguments; the GUI session ends while the child produces ordinary console output, and `startgui` exits with that child status. Its exact `run [arguments]` suffix revalidates only the regular fixed `<project>/main.elf`, launches exactly that already allowlisted project output, forwards at most the existing 127 visible-byte native argument tail, then ends GUI, waits, and returns the child status. Its exact `install` suffix revalidates only the same regular fixed generated output and invokes the established installer with only `<project>/main.elf` and `/apps/<name>/main.elf`; after GUI ends it waits and returns the installer status, preserving the installer's established package-replacement semantics. Its exact `uninstall` suffix revalidates only fixed regular `/apps/<name>/main.elf`, removes only that package output, preserves project source/build and keeps GUI active with a narrow result status; it does not spawn a child. Its exact `clean` suffix revalidates only fixed regular `<project>/main.elf`, removes only that generated output, preserves source/package and keeps GUI active with a narrow result status; it does not spawn a child. Its exact `new [hello|args]` suffix accepts only the bounded project name and the default `hello` or exact `args` starter, creates only fixed `<project>/main.mya`, rejects an existing directory, and removes only its own partial source/directory state after a later ordinary creation or write failure; GUI remains active with a narrow result status. Its exact `remove` suffix immediately revalidates the exact project directory, accepts only `main.mya` and absent `main.elf`, removes the regular source when present and then the empty directory, preserves `/apps/<name>/main.elf`, and keeps GUI active with a narrow result status; it does not claim crash-transactional deletion. An invalid or absent project request reports `UNABLE TO OPEN PROJECT`, a missing or non-regular direct source reports `UNABLE TO OPEN PROJECT SOURCE`, a missing or non-regular generated output reports `UNABLE TO OPEN PROJECT OUTPUT`, and a missing or non-regular package reports `UNABLE TO OPEN PROJECT PACKAGE`, without falling back to a viewer path. Selecting a directory changes the current browser path and keeps the GUI session active, while regular or virtual files are opened in the appropriate viewer or editor. The browser can traverse the complete logical VFS, open readable regular or virtual files, and open small writable regular files in the GUI editor. The `EDIT NOTE` tile retains the default personal-note shortcut, while the general console `edit <absolute-file>` remains the editor for documents above the GUI capacity.

## Persistent user programs

The GUI branch can launch separate MyOS ELF64 files from global persistent application packages at `/apps/<name>/main.elf`. First a built-in initramfs program can be copied into a package, after which it can be launched by a short shell name or an absolute path as a separate ring-3 process. When MYOS DESKTOP opens, it also scans the first 64 `/apps` directory entries, displays up to four package directories that contain a non-empty regular `main.elf`, and exposes each as a mouse-only `OPEN APP` tile. Clicking a package tile revalidates the same bounded path in ring 3, spawns that exact ELF, ends the GUI session and waits for the child so its normal console output remains visible.

```text
install /system/core/apps/hello.elf /apps/hello/main.elf
run hello

install /system/core/apps/argshow.elf /apps/args/main.elf
run args alpha beta
```

| Boundary | Rule |
|---|---|
| Install source | Existing absolute VFS file up to 8 MiB. |
| Target | Only `/apps/<name>/main.elf`; `install` creates the package directory. |
| Loader | Accepts only little-endian x86_64 ELF64 `ET_EXEC` with valid load segments and an entry inside a mapped load segment. |
| Storage | MYPFS004 provides up to 128 persistent file/directory objects, regular files up to 8 MiB and up to six extents per file; `install` copies in 256-byte VFS chunks. |
| Failure | Invalid content, oversized source, invalid path or impossible load are safely rejected; the shell remains usable. |

## MyOS SDK for external build

The `sdk/` directory now contains a compact public SDK for freestanding C11 user programs. It includes `include/myos.h`, a startup object, linker script, a GNU Make template and a validation source `sdk/examples/hello.c`. A program defines `myos_main(uint64_t argc, const char *arguments)` instead of the usual `main`; the startup object calls it and passes the return code to `MYOS_SYS_EXIT`. In the current ABI `argc` is always `1`, and `arguments` is a single NUL-terminated string after the program path.

```bash
make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf
make img
```

The image build adds this reference ELF to the initramfs as `/system/core/examples/sdk/hello.elf`. Therefore the complete regression does not require manual disk image modification:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

The validation program prints a greeting and the accepted argument string. After a fresh BIOS boot the saved `/apps/sdk-hello/main.elf` can be run again with `run sdk-hello`, which validates external build, loader and AHCI-backed persistent storage as an end-to-end path. A detailed public contract, limits and host workflow are provided in [SDK.md](SDK.md).

## Current behavior

| Component | Implemented behavior |
|---|---|
| Renderer | Native direct drawing into the Limine RGB framebuffer without an external GUI runtime. |
| Session ownership | Exactly one GUI owner is allowed at a time; the kernel rejects a second concurrent session. |
| Desktop | Dark desktop, a top status bar with a clickable `X` exit control and compositor-owned `HH:MM:SS` clock, plus a bottom footer with available controls, a bounded `FOCUS HOME/SYSTEM/NOTES/MONITOR` indicator and `TASKS`/`RUN` scheduler snapshots. |
| Launcher | In desktop-home mode, four compact fixed clickable tiles — `SYSTEM`, `NOTES`, `EDIT NOTE` and `FILES` — plus up to four discovered `/apps/<name>/main.elf` package tiles replace the ordinary windows. |
| Windows | Outside launcher mode, three static bounded window records: `SYSTEM`, `NOTES` and `MONITOR`, each with a visible title-bar `X`. |
| Z-order | Focused window is raised to the front; every non-launcher content update also raises `NOTES` so the current viewer or editor is visible. Focus, visibility, layout and content events perform a bounded full redraw composition, whereas ordinary pointer movement updates only the cursor region. |
| Viewer | `NOTES` displays up to 16 KiB (16,384 bytes) of the selected VFS file. A larger readable file is rejected with a GUI capacity status instead of being truncated or copied past the fixed buffer. |
| File loading | `startgui <absolute-path>` reads up to the first 16 KiB (16,384 bytes) of the specified VFS file. |
| File Workspace | `FILES` begins at `/users/myos/`; `startgui projects` enters the same File Workspace directly at `/users/myos/projects`. It supports parent, directory-entry and paged navigation across the complete logical VFS without ending the GUI session, and identifies directories, regular files and virtual entries. Every row has a fixed 12-character visible-name column, a `D`/`F`/`L`/`V` type marker and current logical-VFS byte size with a `B` suffix. `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]`, `[RENAME]` and `[MOVE]` are active only in `/users/myos`, `/temp`, `/system/data` and `/system/config`; `[SEARCH]` is read-only and available in every browsable root. Names are non-empty printable ASCII of up to 63 bytes excluding `/`. New file and folder create one absent empty VFS object; DELETE freezes its named file or empty-directory target until a second `Enter`. COPY streams one regular source of at most 64 KiB to an absent same-directory target and removes only its own partial target on failure. RENAME changes only metadata of one mutable file or directory in its current directory. MOVE accepts a regular-file basename and an absolute existing writable destination directory, retains the basename, rejects an existing target without overwrite, changes metadata only and remains within one persistent move anchor or the `/temp` hierarchy. SEARCH scans at most 128 current-directory entries case-insensitively and renders at most four revalidated results. Read-only roots, invalid names or paths, missing sources, incompatible types and rejected targets report a status without mutation. |
| Desktop home | Bare `startgui` renders the mouse-first `MYOS DESKTOP` launcher; `startgui home` is a compatibility alias. Its fixed system tiles and top-bar exit rectangle remain actionable, and it adds at most four app tiles discovered only from the first 64 `/apps` entries with a verified regular non-empty `main.elf`. It does not scan arbitrary paths or retain unbounded state. |
| Persistent selection | The `NOTES` tile opens the bounded personal-notes route; it selects the next existing note through a directory-scoped VFS enumeration. |
| Installed shortcuts | A package tile represents only `/apps/<name>/main.elf`; its click is revalidated by ring 3, spawns the exact child, ends GUI and waits for that child. Failed revalidation or spawn leaves GUI active with `APP LAUNCH FAILED`. |
| Direct project build | `startgui project <name> build` accepts only the exact bounded project name/suffix, revalidates the fixed regular `main.mya`, and spawns only the established assembler with fixed `main.mya` and `main.elf` paths. After a successful spawn it ends GUI, waits, and exits with the assembler status; a project/source rejection remains in a narrow GUI status. |
| Direct project run | `startgui project <name> run [arguments]` accepts only the same bounded project name plus exact run suffix, revalidates fixed regular `main.elf`, and launches only that project output. Its native argument tail is limited to 127 visible bytes; after successful spawn it ends GUI, waits and exits with the child status, while a rejected output remains in a narrow GUI status. |
| Direct project install | `startgui project <name> install` accepts only the exact bounded name/suffix pair, revalidates fixed regular `main.elf`, and invokes only the established installer with fixed project output and fixed `/apps/<name>/main.elf` target. After successful spawn it ends GUI, waits, and exits with installer status; package replacement remains the existing installer behavior. |
| Direct project uninstall | `startgui project <name> uninstall` accepts only the exact bounded name/suffix pair, revalidates fixed regular `/apps/<name>/main.elf`, and removes only that output. Project source/build remain untouched; it keeps GUI active with a narrow success or rejection status and creates no child process. |
| Direct project clean | `startgui project <name> clean` accepts only the exact bounded name/suffix pair, revalidates fixed regular `<project>/main.elf`, and removes only that generated output. Project source/package remain untouched; it keeps GUI active with a narrow success or rejection status and creates no child process. |
| Direct project creation | `startgui project <name> new [hello|args]` accepts only the bounded name and fixed starter selection, creates only the fixed workspace/source pair, rejects an existing target, and rolls back only its own partial creation state. GUI stays active with a narrow result status and no child process. |
| Direct project removal | `startgui project <name> remove` accepts only the exact bounded name/suffix pair, immediately revalidates the directory, allows only a regular `main.mya` when present and absent `main.elf`, then removes source and empty directory. The installed package remains untouched; GUI stays active with a narrow success or rejection status and no child process. |
| Named launch | `startgui /users/myos/files/notes/<name>` selects a specific personal note; the NOTES title shows the basename of the selected file. |
| Editor entry | The `EDIT NOTE` tile opens a bounded editor for the default personal note; a missing default path starts as an empty draft. FILES opens an existing regular file in the same editor only when its VFS path is writable. Its `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]` and `[RENAME]` controls retain their bounded writable-root behavior; `[MOVE]` relocates one regular file by metadata update to an existing writable absolute destination directory without overwriting it, while `[SEARCH]` remains read-only. |
| Editor input | Printable ASCII is inserted at the caret position; `Enter` inserts a newline; `Backspace` deletes the byte to the left, `Delete` deletes the byte under the caret. |
| Caret and navigation | `Left`/`Right` move the caret by one byte, `Up`/`Down` move by logical lines preserving column, `Home`/`End` go to line boundaries. |
| Bounded scrolling | The renderer displays up to 20 logical newline-separated lines; the viewport automatically follows the caret line. |
| Save and cancel | `Ctrl-S` replaces the selected writable file, writes the draft and returns to its viewer. `Esc`, `Alt+F4` on focused NOTES, or the `NOTES` window `X` cancels the draft and reloads the previously saved content. Read-only paths never enter editor mode. |
| Built-in choices | Clicking `SYSTEM`, `NOTES` or `EDIT NOTE` dispatches fixed bounded actions. `NOTES` opens the bounded personal-notes route and `EDIT NOTE` opens the default personal-note editor. Package choices remain mouse-only and launch verified `/apps/<name>/main.elf`. |
| Focus | Clicking an exposed ordinary window title bar raises it; a body click also retains the existing topmost-window focus behavior. `Alt+Tab` outside the editor moves focus to the next visible window. |
| Hardware pointer | PS/2 mouse relative motion moves a bounded crosshair pointer. A rising-edge left click activates a launcher tile or top-bar `X`; outside launcher mode it first handles the topmost title-bar `X` or title bar, then falls back to window-body focus. |
| Keyboard fallback | `Alt+Tab` cycles focus, `Alt+F4` closes the focused window, `Esc` returns or cancels, and `Ctrl+Q` exits. Temporary single-letter GUI commands, keyboard pointer movement, numeric visibility toggles and layout reset are removed. |
| Visibility | Click a per-window `X` to hide `SYSTEM` or `MONITOR`; the `NOTES` window `X` returns the viewer home or cancels the editor. |
| Reset and exit | `Esc` returns the viewer home and cancels an editor draft. `Ctrl+Q` or the top-bar `X` ends the session and returns to the framebuffer text console. |

In the editor ordinary printable keys become draft text and are not forwarded to the window manager. `Ctrl+S` saves; `Esc` and `Alt+F4` cancel to the viewer; the explicitly global `Ctrl+Q` path discards the draft and exits the GUI session.

## Editor limits and ABI boundary

`NOTES` uses the descriptor `MYOS_GUI_SET_CONTENT = 3` in `MYOS_SYS_GUI_SESSION`. The request accepts one mutually exclusive content mode: editable text, launcher content or browser content. Launcher mode enables four compact fixed launcher hit rectangles and up to four bounded package hit rectangles discovered from `/apps`; browser mode enables bounded parent, previous, entry-row, next-page, create-file, create-directory, named-delete, named-copy, named-rename, file move and search rectangles inside NOTES. The kernel accepts content requests only from the current GUI owner with an active session, copies the request after validating the user buffer mapping and does not retain user pointers. The framebuffer owns its own static copies of title and data. The GUI periodically uses directory-scoped `MYOS_SYS_VFS_LIST` for `/users/myos/files/notes/` and keeps the selected absolute path in bounded static storage. The editor removes the selected file, creates it via the unified VFS and writes up to sixty-four bounded payload chunks at 256-byte-aligned offsets.

| Field or operation | Limit | Purpose |
|---|---:|---|
| `MYOS_GUI_CONTENT_TITLE_MAX` | 112 bytes | NUL-terminated title for the NOTES window; File Workspace uses it for the complete current logical-VFS path. |
| `MYOS_GUI_CONTENT_MAX` | 16 KiB (16,384 bytes) | Maximum viewer content and editor draft length; exactly sixty-four existing 256-byte VFS chunks. |
| `struct myos_gui_content_request` | 16,528 bytes | `length`, `flags`, `cursor`, `viewport`, `title[112]`, `data[16384]`; copied only by the active GUI-session path after a dedicated mapped-range validation. The ordinary 512-byte syscall I/O limit is unchanged. |
| `struct myos_vfs_write_request` | 384 bytes | Unified bounded write request; fits within the syscall user-copy limit of 512 bytes. |
| File read | Up to 16 KiB | Ring 3 uses at most sixty-four bounded 256-byte `MYOS_SYS_VFS_READ` requests; the GUI viewer applies its own 16 KiB content limit. |
| File browser | Four entries per page | FILES starts at `/users/myos/`, re-enumerates the selected logical VFS entry before changing directory or opening it, displays `D`/`F`/`L`/`V`, a 12-character name column and byte size from that entry, and allows traversal to `/` without exposing raw boot media. It sends the complete current directory to the window title; writable roots expose bounded create-file, create-directory, named-delete, named-copy, named-rename and file-only move actions, while every browsable root exposes bounded read-only search. |
| Persistent selection | Up to 64 scanned directory indices | The NOTES shortcut uses `MYOS_SYS_VFS_LIST` only in the notes directory. |
| Launcher app discovery | Up to 64 `/apps` indices and four visible tiles | Kernel and ring 3 independently accept only package directories with a non-empty regular `main.elf`. |
| Selected path | Up to 111 ASCII bytes plus NUL | FILES stores a selected absolute VFS path; direct `startgui <path>` remains viewer-first and `/apps/` retains its executable workflow. |
| Persistent save | Up to 16 KiB per editor update | `VFS_REMOVE`, `VFS_CREATE_FILE`, then up to sixty-four bounded 256-byte `VFS_WRITE` requests only under existing VFS-writable roots: `/users/myos/`, `/temp/`, `/system/data/` and `/system/config/`. `/system/core/`, `/system/live/`, `/apps/` and raw boot media remain non-mutable. |
| Allocation | Static storage | No heap allocations or background operations; selected path, cursor and viewport remain bounded state. |

Text wraps within the internal surface of NOTES. Characters outside printable ASCII are rendered as `?` by the renderer; newline starts the next logical line. In the editor the kernel draws a cyan caret at the index passed by the ring-3 program; the viewport starts at a logical line boundary and keeps the caret line visible in the window up to 20 lines. Content updates trigger a redraw but do not run layout initialization, thus preserving current visibility, focus and window manager z-order. A draft that reaches 16 KiB (16,384 bytes) will not accept new bytes until reduced by `Backspace` or `Delete`.

## Architectural boundaries

`kernel/console/framebuffer.c` owns drawing primitives, static window records, z-order, pointer state, compositor-owned clock/task-status snapshots and a copy of the viewer content. `kernel/drivers/mouse.c` includes the PS/2 auxiliary port, assembles bounded three-byte packets on IRQ12, drops overflows and forwards relative movement with a left-click edge to the framebuffer. `user/startgui.c` owns the ring-3 event loop, editor state, VFS reading, draft cancellation and the persistent save sequence. `kernel/sys/syscall.c` owns owner validation, user-memory copy and framebuffer setter. On termination or forced destruction of the GUI owner the dispatcher closes the GUI session and clears ownership.

| Boundary | Policy |
|---|---|
| Window records | Three static bounded records. |
| Input | Existing scheduler-safe console input path; the PS/2 auxiliary port emits three-byte packets via IRQ12. PS/2 arrows, Home, End and Delete are translated into internal bounded key bytes. The keyboard decoder also maps `Alt+Tab`, `Alt+F4` and `Ctrl+Q` to bounded GUI tokens; in the editor normal keys belong to the draft. |
| Rendering | Full desktop composition runs on content update, focus, visibility or layout change. Ordinary pointer movement restores a bounded 11×11 cursor underlay and draws the cursor in the new place without a full redraw. |
| Files | The viewer reads any available absolute VFS file; the editor modifies the selected note in `/users/myos/files/notes/`. |
| Atomicity | Save removes and recreates the file before writing; MYPFS004 preserves bounded metadata and allocation state, and a full application-level atomic replace is not implemented yet. |
| Mouse hardware | PS/2 relative motion and a left-button edge are implemented. In launcher mode, the kernel maps three fixed tile rectangles, up to four app-tile rectangles and the fixed top-bar `X` rectangle to bounded action characters delivered through the existing scheduler-safe input queue. Outside launcher mode, fixed title-bar and per-window `X` rectangles are checked against the topmost visible record; `NOTES` `X` maps to the existing viewer-home or editor-cancel action, while `SYSTEM` and `MONITOR` update bounded window visibility. Motion remains cursor-only; dragging, wheel and multi-button semantics are not present. |
| General window API | Not implemented; records remain internal to the framebuffer renderer. |

## Milestone verification

Strict build and firmware regressions were run on QEMU Q35 before commit. BIOS and UEFI used the same `myos.img` as an IDE drive, so readback confirms data persistence on the same AHCI-backed persistent partition rather than only in memory for the current run.

| Check | Result |
|---|---|
| `make all img` | Passed without compiler warnings or build errors. |
| `git diff --check` | Passed. |
| BIOS direct named launch | Passed: the console created `disk/todo` with `alpha`; `startgui disk/todo` showed `DISK:TODO` and the content. |
| BIOS cycle | Passed: `N` cycled `DISK:TODO` to `DISK:LOG` and `beta` via existing VFS enumeration. |
| BIOS create on save | Passed: `startgui disk/draft`, then `E`, `x` and `Ctrl-S` created the previously missing selected path; the viewer showed `DISK:DRAFT` and `X`. |
| BIOS selected save | Passed: `E`, append `x` and `Ctrl-S` saved `disk/log`; the title remained `DISK:LOG`, the viewer showed `BETAX`. |
| BIOS PS/2 mouse | Passed: QEMU relative mouse motion moved the crosshair; left-button edge over MONITOR raised that window to the foreground. |
| UEFI PS/2 mouse | Passed: OVMF reported IRQ12 enabled; identical QEMU movement and click moved the crosshair and focused MONITOR. |
| BIOS reliability lifecycle | Passed: `startgui disk/reliability` created and saved `BIOSOK`; `Q` returned with status `0`; user shell `cat` read the file; the same path was relaunched and exited again. |
| UEFI persistent continuity | Passed: OVMF directly read the BIOS-created `BIOSOK`, appended and saved `UEFIOK`, then user-shell `cat` read both lines after GUI exit. |
| Reliability outcome | Passed: no regression observed in GUI owner cleanup, repeatable `startgui`, keyboard input, PS/2 mouse input, return-to-console or AHCI-backed persistence. |
| Legacy persistent ELF baseline | Superseded by MYPFS003: disk/bin ELF workflow was migrated into `/apps/<name>/main.elf`; loader validation remains unchanged. |
| Invalid persistent ELF | Passed: text content at an application `main.elf` target is rejected by the loader without disrupting the user shell. |
| Legacy persistent migration | Passed: prior MYPFS001→MYPFS002 migration remains historical; current MYPFS002→MYPFS003 fixture preservation is recorded below. |
| External MyOS SDK host build | Passed: `make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf` produced a static x86_64 `ELF64 ET_EXEC` with valid loadable segments. |
| MYPFS003 root and runtime | Passed (BIOS): `/system`, `/apps`, `/users/myos`, `/temp`, `/system/live/processes` and `cat /system/live/processes/3/info` returned expected virtual state. |
| MYPFS003 user workflow | Passed (BIOS): `mkdir /users/myos/projects/demo`, persistent write/read, mixed-case `/UsErS/MyOs` lookup, and `/apps` package installation all worked. |
| SDK install, arguments and persistence | Passed: `/system/core/examples/sdk/hello.elf` installed as `/apps/sdk-hello/main.elf`, `run sdk-hello external SDK validation` printed its argument string; a fresh BIOS boot ran the persisted app again. |
| MYPFS003 → MYPFS004 migration | Passed (BIOS): fixture hierarchy and payload migrated through durable `M4MG` recovery marker; `MYPFS004` superblock and cleared journal confirmed before second clean mount. |
| MYPFS002 legacy migration | Passed (BIOS): `disk/note` fixture migrated to `/users/myos/files/notes/note`; `MYPFS004` superblock, cleared journal and second-mount readback confirmed. |
| MYPFS004 large-file I/O | Passed (BIOS): 1 MiB fragmented two-extent pattern write/readback, fresh-mount `wc` of all 1,048,576 bytes, SDK install/run after reboot and UEFI persisted SDK execution. |
| Pointer refresh hardening | Passed: two 1280×800 BIOS framebuffer captures before/after keyboard pointer movement differed in only 726 PPM byte positions, consistent with old/new 11×11 cursor regions; desktop composition remained intact. |
| File Workspace full-path title | Passed: the current logical directory is copied into the expanded 112-byte GUI title field and drawn with compact title spacing. BIOS PPM captures require visible `/users/myos` title glyphs plus title-region transitions after parent and `/system` navigation; UEFI repeats the browser workflow. |
| Automated `make regression` | Passed: the disposable-image harness creates and saves the default GUI note through the mouse `EDIT NOTE` tile, verifies QMP PS/2 `Alt+Tab` focus at MONITOR, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit. It also uses mouse events for the centered `NOTES` tile, the FILES parent row, `/system` directory entry and CPIO-backed `/system/core/apps` route, and PPM title-region transitions proving the full current logical path changes after parent and `/system` navigation, `SYSTEM` and `MONITOR` window `X` controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer. PPM framebuffer transitions validate the visual steps in BIOS and UEFI, including the `Alt+Tab` footer indicator transition from `FOCUS NOTES` to `FOCUS MONITOR` and a discovered installed-app tile that launches the persisted editor-authored package and returns to the shell. The harness also verifies the retained `startgui home` alias, copies the deterministic 16 KiB initramfs fixture through direct shell `cp`, drives GUI save/reload across sixty-four 256-byte VFS transfers and verifies exact BIOS/UEFI persistence, creates and saves zero-byte `/users/myos/guinew` through the File Workspace NEW FILE prompt, creates `/users/myos/guidir` through NEW FOLDER and verifies both UEFI type/size persistence, copies a paced editor-authored 305-byte file with direct shell `cp` across the VFS request boundary, verifies exact data plus direct overwrite refusal and retained `run cp` compatibility rejection, then verifies direct `wc` exact line/word/byte output for a persisted 259-byte file whose final word crosses the 256-byte VFS boundary with retained `run wc` compatibility, direct `grep` output of a short matching line while a matching line crossing the 127-byte retained-line limit is skipped with retained `run grep` compatibility, builds/installs/runs legacy native packages, checks empty and forwarded `args` output, exact `input` match and fallback paths, modular `(250 + 8 - 2) mod 256` `add`/`sub` arithmetic with rejected uninitialized `add`, persisted `MULDIV` multiply/divide arithmetic with rejected `div 0`, persisted `BITWISE` not/and/or byte operations from `240` to `143` with rejected uninitialized `not` and `and 256`, persisted `XOR` byte operation `170 xor 255 xor 85 = 0` with rejected uninitialized `xor` and `xor 256`, persisted `EQ`/`NE` private-slot comparison with rejected uninitialized or slot-`8` `cmp`, plus valid RTC `HH:MM:SS` output, rejects invalid forward-only control flow, then UEFI reads persisted files and copied data, reruns installed input/time/argument/arithmetic packages and enters/exits GUI cleanly. See [RELEASE_STABILIZATION.md](RELEASE_STABILIZATION.md). |
| GUI note and native workflow | Passed: BIOS GUI editor changed persistent note `base` → `base!`; the same note and a BIOS-built native program were read/executed under UEFI. |
| Existing GUI boundaries | Retained: bounded window state, GUI owner checks, direct viewer launch and return to shell. |

Screenshots and brief test findings are located outside the source tree in the local directories `/home/ubuntu/myos-mouse-validation/`, `/home/ubuntu/myos-reliability-validation/` and `/home/ubuntu/myos-disk-elf-validation/`; they are not included in the Git commit.

## Boot UX, inherited from main

Automatic user-space initialization is now implemented and integrated into the GUI branch. After bootstrap the kernel prints a three-second countdown; if not canceled it launches `/init`, after which `startgui` may be invoked immediately. This preserves a fast normal path and a separate diagnostic mode without starting the GUI.

| Post-boot scenario | Implemented behavior |
|---|---|
| Normal boot | The kernel groups diagnostics into four stage headers, prints a countdown and automatically launches `/init` after **3 seconds**; the framebuffer is cleared before the user shell. |
| Cancel | Pressing `K` during the countdown cancels auto-init; the cancel key is not delivered to the user shell. |
| Kernel shell | After `K` the system remains in the diagnostic kernel shell. The `init` command manually starts the same user shell. |
| Init failure | If `/init` is missing or automatic loading fails, the kernel prints diagnostics and remains in the kernel shell without a retry loop. |
| Input source | The cancel path works via existing PS/2 keyboard and serial console input paths. |
| Verification | BIOS normal boot, PS/2 `K` cancellation, manual `init`, isolated no-init fallback and UEFI normal boot with a clean user-shell framebuffer passed on QEMU Q35. |

The original GUI preview boundary remains fixed at immutable tag `v0.12.2-gui-preview`; the earlier `v0.13.0-gui-rc.1` tag is retained as historical evidence, while `v0.13.1-gui-preview.1` is the current public QEMU-validated preview. It does not claim physical-PC validation or imply a merge into `main`. The current `feature/gui` branch contains the MYPFS004 hierarchy, 8 MiB dynamic large-file storage, `/apps` ELF execution, the MyOS SDK for external build with public bounded VFS wrappers and its live no-overwrite `cp` developer tool, and a restricted in-OS `asm`/`build` workflow with bounded `args` forwarding from `run <name> [arguments]`, labels, explicit `set` values, initialized modular `add`/`sub`/`mul` byte arithmetic, safe unsigned `div`, bounded private-slot `cmp`, eight-slot `store`/`load` byte variables, single-byte `input`, RTC `time`, exact `jump_if <0..255>` comparison and forward-only branches. Its generated image adds only a fixed private 32-byte RW data segment: an entry argument pointer, input/time scratch storage and eight private variable bytes; `add bl, imm8`, `sub bl, imm8` and the low-byte result of `mul` keep the initialized accumulator bounded to modulo-256 semantics; `div` accepts only a nonzero `1..255` divisor and produces an unsigned byte quotient; `cmp <0..7>` reads only a private slot and turns equality into zero or inequality into one for existing conditional branches. The branch also contains the bounded mouse-first desktop launcher entered by `startgui` (with `startgui home` as an alias), File Workspace current-directory titles carried in the fixed 112-byte GUI title field and drawn with compact spacing at the supported 1280×800 geometry, per-window title-bar raise and close controls, the general console [Text Editor](TEXT_EDITOR.md) and cursor-only GUI pointer refresh. The GUI editor remains a notes-focused feature; direct `edit <absolute-file>` is the general file editor. The directory layout was jointly agreed with the user and recorded in [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md); future native-platform work must preserve the completed bounded execution contract.
