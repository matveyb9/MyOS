# MyOS SDK: external development of user-space programs

> **🌐 LANGUAGE / ЯЗЫК:** [🇷🇺 РУССКИЙ](SDK_RU.md) / **🇺🇸 ENGLISH**


## Purpose

**MyOS SDK** provides a minimal, reproducible path for building custom MyOS user programs on a host machine. The SDK produces a static `x86_64 ELF64 ET_EXEC` binary that is loaded into ring 3 at address `0x400000` and does not require changing or rebuilding the kernel source.

At this milestone the SDK is intentionally compact: it does not include POSIX, a dynamic linker, a standard C library, or a native C compiler inside MyOS. A restricted native assembler `asm`/`build` is already available as a separate in-OS workflow, but the SDK remains an external path for freestanding C11 with direct use of a small public syscall ABI.

| Component | Path | Purpose |
|---|---|---|
| Public header | `sdk/include/myos.h` | ABI version, safe thin wrappers for available syscalls and the `myos_main` contract. |
| Startup object | `sdk/lib/crt0.c` | Implements `_start`, calls the program and passes its return code to `MYOS_SYS_EXIT`. |
| Linker script | `sdk/myos-user.ld` | Produces a static ELF64 with entry point `_start` and loadable segments for the MyOS loader. |
| Build template | `sdk/Makefile` | Builds a given C file into a ready MyOS ELF. |
| Validation example | `sdk/examples/hello.c` | Prints a message and the received argument string. |
| Practical SDK tool | `sdk/examples/cp.c` | Copies an existing regular file to a new absolute target through only the public SDK VFS wrappers. The image stages it as `/system/core/apps/cp.elf`. |

## Requirements and build

Builds are performed on the host with `gcc`, `ld` from GNU binutils and GNU Make. From the repository root run:

```bash
make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf
file sdk/build/sdk-hello.elf
readelf -h -l sdk/build/sdk-hello.elf
```

The expected result from `file` is **ELF 64-bit executable, x86-64, statically linked**. `readelf` should show type `EXEC`, architecture `Advanced Micro Devices X86-64`, an entry point in the user range and loadable segments. For a quick run of the standard example the abbreviated command `make -C sdk` is allowed; it will produce `sdk/build/hello.elf`.

The template applies freestanding C11 flags, disables stack protector, PIC/PIE, the red zone and SIMD register state preservation. Therefore a program must not depend on libc, host files, or a conventional `main()`.

## Program contract and public ABI

Instead of the normal `main(int argc, char **argv)` the application defines a single function:

```c
#include <myos.h>

int myos_main(uint64_t argc, const char *arguments) {
    myos_write_text("Hello from a MyOS program!\n");
    return 0;
}
```

The startup object receives the ABI entry `_start(uint64_t argc, const char *arguments)`, calls `myos_main` and terminates the task with its return code. Currently the shell passes **exactly one** textual argument: `argc` is always `1`, and `arguments` points to a NUL-terminated string after the program path. An empty string indicates no arguments. This is the current contract, not POSIX `argv[]`.

| Item | Current rule |
|---|---|
| ABI version | `MYOS_ABI_VERSION = 0x00010000`. |
| Program entry | Required function `int myos_main(uint64_t argc, const char *arguments)`. |
| Termination | `myos_main` return code is passed to `MYOS_SYS_EXIT`; explicit exit is possible via `myos_exit(status)`. |
| Text output | `myos_write()` performs a bounded write; `myos_write_text()` writes a NUL-terminated ASCII text to standard output. |
| Additional wrappers | `myos_getpid()` and `myos_ticks()` are available as direct read-only syscall wrappers. |
| VFS subset | `myos_vfs_read()`, `myos_vfs_create_file()`, `myos_vfs_write()` and `myos_vfs_remove()` use public fixed-size request structures. Reads and writes are limited to 256 bytes per request. |
| Memory and runtime | No libc allocation, constructors, dynamic linking or floating-point runtime. |
| Format | Only little-endian `x86_64 ELF64 ET_EXEC`; ELF32, PIE and dynamic ELF are not supported. |

> **ABI compatibility.** The version number fixes the first public SDK contract. If a syscall wrapper or the entry convention changes incompatibly the SDK will receive a new ABI version; the old public header will not be silently overridden.

## Testing in MyOS

The standard image build adds the validation example into the read-only initramfs as `/system/core/examples/sdk/hello.elf`. Build the raw disk image and attach it to QEMU as an IDE drive so that the persistent AHCI path is used in the supported configuration:

```bash
make img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none \
  -no-reboot -no-shutdown
```

After automatic login to `[myos]$` stage the ELF into the executable namespace and run it:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

The expected output contains `Hello from MyOS SDK!` and the line `Arguments: external SDK validation`. `install` creates a persistent package directory `/apps/sdk-hello/` and copies the ELF as `main.elf`; `run` creates a new user task and passes the remainder of the command line as arguments. After a reboot it is sufficient to run `run sdk-hello persisted`: reinstallation is not required.

The image also stages the SDK-built practical copy tool as the live app `cp`. It needs two absolute paths; its destination must not yet exist and its parent directory must already exist. This conservative rule prevents an accidental overwrite or source loss. For example:

```text
write /users/myos/files/source.txt MyOS SDK copy
run cp /users/myos/files/source.txt /users/myos/files/target.txt
cat /users/myos/files/target.txt
```

The tool reads and writes in 256-byte requests, supports empty source files and files up to the existing 8 MiB regular-file ceiling, and removes only its newly-created partial target when a copy fails.

| Limitation | Current value |
|---|---:|
| Persistent VFS objects | Up to 128 files and directories in MYPFS004. |
| Maximum size of one persistent regular file | 8 MiB. |
| Persistent executable target | `/apps/<name>/main.elf`; the short name `<name>` is resolved by the shell to this target. |
| Length of absolute program path | Up to 111 visible ASCII bytes plus NUL terminator. |
| Length of passed arguments string | Up to 127 visible bytes plus NUL terminator. |
| Initramfs staging path of the example | `/system/core/examples/sdk/hello.elf`. |
| Live SDK tool path | `/system/core/apps/cp.elf`, resolved as `run cp`. |
| `cp` destination rule | Absolute path, absent target, and an already-existing parent directory; existing targets are never overwritten. |

## How to replace the example with your own program

The path `sdk/examples/hello.c` is a normal SDK source file. You may modify it or specify a different file with `APP`; for example, `make -C sdk APP=apps/status.c OUT=sdk/build/status.elf`. For reproducible verification the reference image builds the sample into `/system/core/examples/sdk/hello.elf`. Replace the sample source, then run `make img` so the updated ELF appears at that path, after which use `install /system/core/examples/sdk/hello.elf /apps/<name>/main.elf` and `run <name>`.

MYPFS004 provides a real file hierarchy and dynamic multi-extent regular files: sources and local build outputs are intended for `/users/myos/projects/`, globally installed apps are for `/apps/`, and personal data and configuration are for `/users/myos/data/` and `/users/myos/config/`. The first in-OS assembly workflow is implemented via `build`; its restricted syntax and package workflow are described in [NATIVE_BUILD.md](NATIVE_BUILD.md).

## Milestone validation

Validation was performed on the `gui/bringup` branch in QEMU Q35 BIOS with raw `myos.img` attached via `-drive if=ide,format=raw,file=myos.img`.

| Check | Result |
|---|---|
| Host build | `make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf` completed with no warnings or errors. |
| ELF inspection | A statically linked `ELF64 ET_EXEC` for x86-64 was produced with entry `0x40005f` and loadable text/rodata segments. |
| Image build | `make img` adds the SDK sample as `/system/core/examples/sdk/hello.elf`. |
| Install and run | `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`, then `run sdk-hello external SDK validation` printed the greeting and the full argument string; status `0`. |
| Persistence | After a fresh BIOS boot `run sdk-hello persisted` successfully runs the previously installed ELF from the MYPFS004 application package. |
| UEFI execution | OVMF boot with the same `myos.img` successfully ran the persisted app with `run sdk-hello uefi`. |
| SDK VFS copy | The SDK-built live `cp` copied an editor-authored 305-byte persistent file across the 256-byte request boundary, rejected a second overwrite attempt, and its exact target data persisted through UEFI. |

## Not included in this milestone

The SDK does not add 32-bit compatibility, a native C compiler, dynamic linking, process `argv[]` or a package manager. The VFS subset deliberately omits directory creation, listing, rename, metadata, overwrite flags and arbitrary I/O buffering. The unified hierarchy and MYPFS004 large-file storage are already implemented; symbolic links, GUI shortcuts and per-user app installation remain future extensions. The current GUI branch and immutable tags remain unchanged.
