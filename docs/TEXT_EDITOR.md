# MyOS Text Editor

<p align="center">
  <a href="TEXT_EDITOR_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Status:** implemented and validated in `feature/gui`. `edit` is a small console text editor for ordinary VFS files and multi-line `.mya` sources. It is intentionally complementary to the GUI editor: File Workspace v1 can open a selected small writable file from `startgui` → `FILES`, while the console editor remains the larger general-purpose tool.

## Start editing

Use an absolute path. The parent directory must already exist. An existing file opens at its final byte; a missing file is created as an empty text document.

```text
edit /users/myos/projects/hello.mya
```

The editor redraws a compact text viewport after each input event. Its visible `|` marker is the byte cursor; it is not stored in the file.

| Control | Action |
|---|---|
| Printable ASCII | Insert a character at the cursor. |
| `Enter` | Insert a newline. |
| `Left`, `Right` | Move one byte. |
| `Home`, `End` | Move to the start or end of the current line. |
| `Up`, `Down` | Move between lines while keeping the column where possible. |
| `Backspace`, `Delete` | Delete the preceding or current byte. |
| `Ctrl-S` | Replace the file with the edited document, then exit. |
| `Ctrl-Q` or `Esc` | Exit without saving the in-memory edits. |

The command remains compatible with the older form `run edit <absolute-file>`, but direct `edit <absolute-file>` is the normal workflow. `help edit` shows the same command and key summary in the user shell.

## Writing a native program

The editor is a general text tool; `.mya` authoring is its first development-oriented use. Create a source with real line breaks, save it, then build and install it as usual.

```text
edit /users/myos/projects/zero.mya
# Type and save with Ctrl-S:
set 0
jump_if_zero done
write "bad\n"
label done:
write "editor-built\n"
exit 44

build /users/myos/projects/zero.mya /users/myos/projects/zero.elf
install /users/myos/projects/zero.elf /apps/editor-zero/main.elf
run editor-zero
```

The program prints only `editor-built` and returns status `44`. See [Native Build](NATIVE_BUILD.md) for the complete restricted `.mya` grammar and safety bounds.

## Bounds and save behavior

| Item | Current rule |
|---|---|
| Console editable document | At most **4,096 bytes** in memory. |
| Console file input/output | Read and write use bounded 256-byte VFS requests. |
| Supported content | Printable ASCII, newline and tab are suitable text content; the editor is not a binary-file tool. |
| File locations | Any mutable absolute VFS file with an existing parent directory. The console workflow follows VFS write policy; File Workspace GUI editing is further limited to selected existing regular files below `/users/myos/`, `/temp/`, `/system/data/` or `/system/config/`. |
| Save model | `Ctrl-S` replaces the target through remove/create and bounded writes. There is no undo, recovery journal, atomic rename or concurrent-edit coordination yet. |

The console editor’s 4 KiB limit is intentionally below the 128 KiB persistent-file open snapshot and far below the 8 MiB MYPFS004 file ceiling. The separate File Workspace GUI editor accepts a fixed **16 KiB (16,384-byte)** document and loads or saves it through up to sixty-four unchanged 256-byte VFS requests. It remains a mouse-first editor for selected existing writable files, while the console editor remains the general-purpose path for every mutable absolute VFS file. Both stay bounded, deterministic and practical for notes, configuration and native sources; larger viewing and editing remain separate future work.

## Validation

`make regression` creates and saves a two-line ordinary text file in the console editor, verifies exact BIOS readback, verifies a 16 KiB GUI editor load-save-readback through sixty-four VFS chunks seeded from a deterministic initramfs fixture, authors a multi-line conditional `.mya` file in the editor, builds and runs its installed package, then repeats ordinary-text readback, the exact 16 KiB GUI payload and native-package execution after UEFI/OVMF boot. The regression uses a disposable image and does not replace the remaining physical-PC release gate.

For broader shell behavior, see the [User Guide](USER_GUIDE.md). The File Workspace GUI editor and its logical-VFS navigation are described in [GUI Bring-up](GUI_BRINGUP.md).
