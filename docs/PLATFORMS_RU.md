# Платформы: сборка и запуск MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="PLATFORMS.md">🇺🇸 ENGLISH</a>
</p>


Этот документ объясняет, как использовать исходники MyOS на разных host-платформах. **Target остаётся x86_64 PC**; host-платформа — это компьютер, на котором вы собираете artifacts и запускаете QEMU.

> Самый надёжный путь для MyOS сегодня — Linux или Ubuntu внутри WSL 2. Он использует GNU toolchain, GNU Make, Linux utilities и QEMU exactly as the project Makefile expects.

## Матрица поддержки

| Host platform | Build | QEMU run | Support level | Recommended use |
|---|---:|---:|---|---|
| Ubuntu/Debian Linux x86_64 | Yes | Yes | **Verified** | Основной development path. |
| Other mainstream Linux | Yes | Yes | Supported with package-name adjustments | Fedora, Arch, openSUSE and similar. |
| Windows 10/11 + WSL 2 Ubuntu | Yes | Yes | **Recommended Windows path** | Build/run in Linux environment from Windows. |
| Windows native + MSYS2 | Expected to be possible | Yes | Experimental | For users who specifically need native Windows tools. |
| macOS + Homebrew | Requires cross-toolchain work | Yes | Experimental | Prefer a Linux VM/container for reproducibility. |
| BSD/other Unix-like host | Requires GNU-compatible toolchain | Depends | Community/experimental | Use a Linux VM if unsure. |

## Common source checkout

On every platform, first obtain the project and enter its root:

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
```

If you downloaded a GitHub ZIP archive, unpack it and open its root. ZIP contains source only: it does **not** contain `.git`, branch references or tags. Use `git clone` when you need branch selection, history or future `git pull` updates.

The published default branch is the console-maintenance `main`. To build the current GUI development line after cloning, select it explicitly:

```bash
git switch gui/bringup
```

For every verified path, the main build command is:

```bash
make all img
```

This creates `myos.iso` and `myos.img`. The raw `myos.img` is the required artifact for persistent MYPFS004 testing through `/system`, `/apps`, `/users/myos` and `/temp`.

## Linux: Ubuntu and Debian

### Install prerequisites

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

`qemu-system-x86` is the Debian/Ubuntu package family recommended by QEMU for full system emulation. [1]

### Build and run BIOS

```bash
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

MyOS automatically enters the `[myos]$` user shell after a three-second countdown. Press `K` during that countdown to remain at the `kernel>` diagnostic shell, where `init` still starts the user shell manually.

### Run UEFI/OVMF

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

Package paths can vary on non-Debian distributions. If OVMF is installed elsewhere, replace both paths with the paths reported by your package manager.

### Other Linux distributions

QEMU documents package-manager installation paths for Arch, Fedora, Gentoo, RHEL/CentOS and SUSE. [1] Install equivalents of the following categories:

| Requirement | Examples |
|---|---|
| GNU compiler and linker | `gcc`, `binutils`, `make` or development group. |
| Assembler | `nasm`. |
| Image tools | `xorriso`, `mtools`, `gptfdisk`/`gdisk`. |
| Emulator | QEMU system x86_64 package. |
| UEFI test firmware | OVMF/edk2-ovmf package. |

On Fedora, QEMU’s own download page lists `dnf install @virtualization` as a broad QEMU installation route. [1] Package names for the remaining build utilities differ by release, so confirm them with `dnf search`, `pacman -Ss` or your distribution documentation.

## Windows 10/11: recommended WSL 2 path

### Why WSL 2

WSL runs a GNU/Linux distribution and Linux command-line tools directly on Windows. Microsoft documents `wsl --install` for Windows 10 version 2004+ and Windows 11; it enables the required features and installs Ubuntu by default. [2]

### Install WSL

Open **PowerShell as Administrator** and run:

```powershell
wsl --install
```

Restart when Windows asks. Then open the Ubuntu application from the Start menu, create the Linux username/password requested on first start, and perform the Linux setup inside that terminal.

### Build inside Ubuntu (WSL)

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf git
cd ~
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch gui/bringup
make all img
```

For the most predictable performance and Unix file behavior, keep the repository inside the Linux home directory (`~/myos`) rather than a mounted Windows drive such as `/mnt/c/...`.

### Run in WSL

Start with serial-only QEMU, because it does not depend on graphical integration:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

MyOS enters `[myos]$` automatically after the three-second countdown. Press `K` only when the diagnostic `kernel>` shell is needed, then use `init` for manual user-shell entry. QEMU display-window support depends on your WSL/Windows graphics configuration; if it is unavailable, the serial route above remains sufficient for the console OS.

## Windows native: MSYS2 path

Native Windows use is available as an **experimental** path. Use WSL unless you have a reason to build under Windows tools.

1. Install [MSYS2](https://www.msys2.org/) and open the **UCRT64** terminal. MSYS2 provides a native Windows build environment and uses `pacman` for packages. [3]
2. Update the environment:

   ```bash
   pacman -Syu
   ```

   Close/reopen the terminal if MSYS2 asks you to do so, then run the command again until fully updated.

3. Install a GNU build environment and QEMU. QEMU documents this UCRT64 package name for native Windows QEMU: [1]

   ```bash
   pacman -S --needed \
     git make nasm xorriso mtools gptfdisk \
     mingw-w64-ucrt-x86_64-gcc \
     mingw-w64-ucrt-x86_64-binutils \
     mingw-w64-ucrt-x86_64-qemu
   ```

4. Clone the repository into an ordinary directory without spaces, enter it and run:

   ```bash
   make all img
   ```

### Native Windows caveats

The MyOS Makefile uses Unix shell commands, GNU linker behavior, `mtools`, `sgdisk` and a downloaded Limine build. Exact MSYS2 package names and path behavior may evolve. If a tool is unavailable or `make` fails, use WSL 2 rather than modifying the MyOS source just to work around host differences.

QEMU’s official download page also links to third-party Windows installers, but installing QEMU alone does not provide the complete GNU image-building environment that MyOS requires. [1]

## macOS: Homebrew path

QEMU can be installed through Homebrew with:

```bash
brew install qemu
```

Both QEMU and Homebrew document this installation route. [1] [4]

For the image utilities, install:

```bash
brew install nasm xorriso mtools gptfdisk
```

### Important macOS limitation

MyOS is built as an **x86_64 ELF freestanding kernel**. Apple’s default compiler/linker produce Mach-O binaries, so a standard macOS compiler setup is not automatically sufficient to satisfy the current Makefile’s GNU ELF expectations. A working native macOS build therefore needs an x86_64-elf cross GCC/binutils toolchain and compatible package paths, which is not yet a verified MyOS host configuration.

The practical macOS recommendation is to run an Ubuntu VM or container for the build and use Homebrew QEMU for emulation only if the local setup is known to work. Document any confirmed native macOS setup in a future update to this file.

## Other Unix-like hosts

On FreeBSD, OpenBSD, NetBSD or similar systems, use a GNU-compatible x86_64-elf cross toolchain, NASM, GNU Make, QEMU, `xorriso`, `mtools` and `sgdisk` equivalents. This has not been validated by the project. A Linux VM is the safer choice when reproducible results matter.

## Physical USB media

Physical USB boot is host-independent in principle, but the write command differs by platform. In every case, use only `myos.img` and only a dedicated disposable USB device.

| Host | Typical command form | Warning |
|---|---|---|
| Linux / WSL with direct device access | `sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress` | Verify with `lsblk`; this erases the entire disk. |
| macOS | `sudo dd if=myos.img of=/dev/rdiskN bs=4m` | Identify device with `diskutil list`; unmount it first. |
| Windows | Use a raw-image writer that supports `.img` files | Verify the selected drive carefully; do not use a system disk. |

> Never run a destructive write command until you have positively identified the target USB device. The MyOS team cannot recover data erased by choosing the wrong device.

## References

[1]: https://www.qemu.org/download/ "QEMU — Download and platform installation guidance"
[2]: https://learn.microsoft.com/en-us/windows/wsl/install "Microsoft Learn — Install WSL"
[3]: https://www.msys2.org/ "MSYS2 — Installation and package environment"
[4]: https://formulae.brew.sh/formula/qemu "Homebrew Formulae — qemu"
