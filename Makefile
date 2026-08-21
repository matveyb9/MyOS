.DEFAULT_GOAL := all

PROJECT        := myos
BUILD_DIR      := build
KERNEL         := $(BUILD_DIR)/kernel.elf
USER_BUILD_DIR := $(BUILD_DIR)/user
USER_INIT      := $(USER_BUILD_DIR)/init
USER_HELLO     := $(USER_BUILD_DIR)/hello
USER_SLEEPER   := $(USER_BUILD_DIR)/sleeper
USER_ORPHANER  := $(USER_BUILD_DIR)/orphaner
USER_SAFETY    := $(USER_BUILD_DIR)/safety
USER_ARGSHOW   := $(USER_BUILD_DIR)/argshow
USER_CALC      := $(USER_BUILD_DIR)/calc
USER_PIPEWRITE := $(USER_BUILD_DIR)/pipewrite
USER_PIPEREAD  := $(USER_BUILD_DIR)/piperead
USER_WC        := $(USER_BUILD_DIR)/wc
USER_GREP      := $(USER_BUILD_DIR)/grep
USER_EDIT      := $(USER_BUILD_DIR)/edit
USER_STARTGUI  := $(USER_BUILD_DIR)/startgui
USER_INSTALL   := $(USER_BUILD_DIR)/install
USER_ASM       := $(USER_BUILD_DIR)/asm
USER_TREE      := $(USER_BUILD_DIR)/tree
USER_FIND      := $(USER_BUILD_DIR)/find
USER_STACKPROBE := $(USER_BUILD_DIR)/stackprobe
USER_HEAD      := $(USER_BUILD_DIR)/head
SDK_HELLO      := $(BUILD_DIR)/sdk/sdk-hello.elf
SDK_CP         := $(BUILD_DIR)/sdk/cp.elf
USER_MOTD      := user/motd.txt
INITRAMFS      := $(BUILD_DIR)/initramfs.cpio
ISO_ROOT       := $(BUILD_DIR)/iso_root
LIMINE_DIR     := third_party/limine-binary
LIMINE_URL     := https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz
OVMF_CODE       := /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS       := /usr/share/OVMF/OVMF_VARS_4M.fd

CC             := gcc
LD             := ld
NASM           := nasm

WARNINGS       := -Wall -Wextra -Werror -Wshadow -Wconversion -Wundef
CFLAGS         := -std=gnu11 -O0 -g $(WARNINGS) \
                 -ffreestanding -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
                 -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
                 -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mcmodel=kernel \
                 -mno-mmx -mno-sse -mno-sse2 \
                 -Iinclude
NASMFLAGS       := -f elf64 -g -F dwarf -Wall
USER_CFLAGS     := -std=gnu11 -O0 -g -Wall -Wextra -Werror -Wshadow -Wconversion -Wundef -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mcmodel=small -mno-mmx -mno-sse -mno-sse2 -Iinclude
LDFLAGS         := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T boot/linker.ld

C_SOURCES      := $(shell find kernel -name '*.c' | sort)
ASM_SOURCES    := $(shell find kernel -name '*.asm' | sort)
OBJECTS        := $(patsubst %.c,$(BUILD_DIR)/obj/%.c.o,$(C_SOURCES)) \
                  $(patsubst %.asm,$(BUILD_DIR)/obj/%.asm.o,$(ASM_SOURCES))

.PHONY: all kernel initramfs iso img run run-graphic run-uefi run-uefi-graphic smoke regression release-check debug clean distclean inspect help sdk-stage

all: iso
kernel: $(KERNEL)
initramfs: $(INITRAMFS)
iso: $(PROJECT).iso
img: $(PROJECT).img

$(BUILD_DIR)/obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

$(KERNEL): $(OBJECTS) boot/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(USER_INIT): user/init.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/init.c -o $(USER_BUILD_DIR)/init.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/init.o -o $@

$(USER_HELLO): user/hello.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/hello.c -o $(USER_BUILD_DIR)/hello.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/hello.o -o $@

$(USER_SLEEPER): user/sleeper.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/sleeper.c -o $(USER_BUILD_DIR)/sleeper.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/sleeper.o -o $@

$(USER_ORPHANER): user/orphaner.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/orphaner.c -o $(USER_BUILD_DIR)/orphaner.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/orphaner.o -o $@

$(USER_SAFETY): user/safety.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/safety.c -o $(USER_BUILD_DIR)/safety.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/safety.o -o $@

$(USER_ARGSHOW): user/argshow.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/argshow.c -o $(USER_BUILD_DIR)/argshow.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/argshow.o -o $@

$(USER_CALC): user/calc.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/calc.c -o $(USER_BUILD_DIR)/calc.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/calc.o -o $@

$(USER_PIPEWRITE): user/pipewrite.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/pipewrite.c -o $(USER_BUILD_DIR)/pipewrite.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/pipewrite.o -o $@

$(USER_PIPEREAD): user/piperead.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/piperead.c -o $(USER_BUILD_DIR)/piperead.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/piperead.o -o $@

$(USER_WC): user/wc.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/wc.c -o $(USER_BUILD_DIR)/wc.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/wc.o -o $@

$(USER_GREP): user/grep.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/grep.c -o $(USER_BUILD_DIR)/grep.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/grep.o -o $@

$(USER_EDIT): user/edit.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/edit.c -o $(USER_BUILD_DIR)/edit.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/edit.o -o $@

$(USER_STARTGUI): user/startgui.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/startgui.c -o $(USER_BUILD_DIR)/startgui.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/startgui.o -o $@

$(USER_INSTALL): user/install.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/install.c -o $(USER_BUILD_DIR)/install.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/install.o -o $@

$(USER_ASM): user/asm.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/asm.c -o $(USER_BUILD_DIR)/asm.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/asm.o -o $@

$(USER_TREE): user/tree.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/tree.c -o $(USER_BUILD_DIR)/tree.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/tree.o -o $@

$(USER_FIND): user/find.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/find.c -o $(USER_BUILD_DIR)/find.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/find.o -o $@

$(USER_STACKPROBE): user/stackprobe.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/stackprobe.c -o $(USER_BUILD_DIR)/stackprobe.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/stackprobe.o -o $@

$(USER_HEAD): user/head.c user/linker.ld include/syscall.h
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c user/head.c -o $(USER_BUILD_DIR)/head.o
	$(LD) -m elf_x86_64 -nostdlib -static -T user/linker.ld $(USER_BUILD_DIR)/head.o -o $@

$(SDK_HELLO): sdk/examples/hello.c sdk/Makefile sdk/lib/crt0.c sdk/include/myos.h sdk/myos-user.ld
	$(MAKE) -C sdk APP=$(abspath sdk/examples/hello.c) OUT=$(abspath $@)

$(SDK_CP): sdk/examples/cp.c sdk/Makefile sdk/lib/crt0.c sdk/include/myos.h sdk/myos-user.ld
	$(MAKE) -C sdk APP=$(abspath sdk/examples/cp.c) OUT=$(abspath $@)

sdk-stage: $(SDK_HELLO) $(SDK_CP)

$(INITRAMFS): $(USER_INIT) $(USER_HELLO) $(USER_SLEEPER) $(USER_ORPHANER) $(USER_SAFETY) $(USER_ARGSHOW) $(USER_CALC) $(USER_PIPEWRITE) $(USER_PIPEREAD) $(USER_WC) $(USER_GREP) $(USER_EDIT) $(USER_STARTGUI) $(USER_INSTALL) $(USER_ASM) $(USER_TREE) $(USER_FIND) $(USER_STACKPROBE) $(USER_HEAD) $(SDK_HELLO) $(SDK_CP) $(USER_MOTD) user/gui_4k_fixture.txt tools/mkcpio.py Makefile
	python3 tools/mkcpio.py $@ system/core/apps/init.elf $(USER_INIT) system/core/apps/hello.elf $(USER_HELLO) system/core/apps/sleeper.elf $(USER_SLEEPER) system/core/apps/orphaner.elf $(USER_ORPHANER) system/core/apps/safety.elf $(USER_SAFETY) system/core/apps/argshow.elf $(USER_ARGSHOW) system/core/apps/calc.elf $(USER_CALC) system/core/apps/pipewrite.elf $(USER_PIPEWRITE) system/core/apps/piperead.elf $(USER_PIPEREAD) system/core/apps/wc.elf $(USER_WC) system/core/apps/grep.elf $(USER_GREP) system/core/apps/edit.elf $(USER_EDIT) system/core/apps/startgui.elf $(USER_STARTGUI) system/core/apps/install.elf $(USER_INSTALL) system/core/apps/asm.elf $(USER_ASM) system/core/apps/tree.elf $(USER_TREE) system/core/apps/find.elf $(USER_FIND) system/core/apps/stackprobe.elf $(USER_STACKPROBE) system/core/apps/head.elf $(USER_HEAD) system/core/examples/sdk/hello.elf $(SDK_HELLO) system/core/apps/cp.elf $(SDK_CP) system/core/resources/motd.txt $(USER_MOTD) system/core/resources/gui-4k.txt user/gui_4k_fixture.txt

$(LIMINE_DIR)/limine:
	@rm -rf $(LIMINE_DIR)
	@mkdir -p third_party
	curl -L --fail --retry 3 $(LIMINE_URL) | tar -xz -C third_party
	$(MAKE) -C $(LIMINE_DIR)

$(PROJECT).iso: $(KERNEL) $(INITRAMFS) $(LIMINE_DIR)/limine boot/limine.conf
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp $(INITRAMFS) $(ISO_ROOT)/boot/initramfs.cpio
	cp boot/limine.conf $(ISO_ROOT)/boot/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin --efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $@
	$(LIMINE_DIR)/limine bios-install $@

$(PROJECT).img: $(KERNEL) $(INITRAMFS) $(LIMINE_DIR)/limine boot/limine.conf
	@rm -f $@
	dd if=/dev/zero of=$@ bs=1M count=128 status=none
	PATH=$$PATH:/usr/sbin:/sbin sgdisk -og $@
	PATH=$$PATH:/usr/sbin:/sbin sgdisk -a 1 -n 1:34:2047 -t 1:ef02 -c 1:'MyOS BIOS boot' -n 2:2048:67583 -t 2:ef00 -c 2:'MyOS EFI' -n 3:67584:262110 -t 3:8300 -c 3:'MyOS data' $@
	$(LIMINE_DIR)/limine bios-install $@ 1 --no-gpt-to-mbr-isohybrid-conversion
	mformat -i $(PROJECT).img@@1M
	mmd -i $(PROJECT).img@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(PROJECT).img@@1M $(KERNEL) ::/boot/kernel.elf
	mcopy -i $(PROJECT).img@@1M $(INITRAMFS) ::/boot/initramfs.cpio
	mcopy -i $(PROJECT).img@@1M boot/limine.conf ::/boot/limine.conf
	mcopy -i $(PROJECT).img@@1M $(LIMINE_DIR)/limine-bios.sys ::/boot/limine/limine-bios.sys
	mcopy -i $(PROJECT).img@@1M $(LIMINE_DIR)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

run: $(PROJECT).iso
	qemu-system-x86_64 -machine q35 -m 256M -cdrom $(PROJECT).iso -boot d \
		-serial stdio -display none -no-reboot -no-shutdown

run-graphic: $(PROJECT).iso
	qemu-system-x86_64 -machine q35 -m 256M -cdrom $(PROJECT).iso -boot d \
		-serial stdio -no-reboot -no-shutdown

$(BUILD_DIR)/OVMF_VARS.fd:
	@mkdir -p $(BUILD_DIR)
	cp $(OVMF_VARS) $@

run-uefi: $(PROJECT).iso $(BUILD_DIR)/OVMF_VARS.fd
	qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD_DIR)/OVMF_VARS.fd \
		-cdrom $(PROJECT).iso -boot d -serial stdio -display none -no-reboot -no-shutdown

run-uefi-graphic: $(PROJECT).iso $(BUILD_DIR)/OVMF_VARS.fd
	qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD_DIR)/OVMF_VARS.fd \
		-cdrom $(PROJECT).iso -boot d -serial stdio -no-reboot -no-shutdown

smoke: $(PROJECT).img tests/run_qemu_boot_smoke.sh
	tests/run_qemu_boot_smoke.sh $(PROJECT).img

regression: $(PROJECT).img tests/run_interactive_regression.py
	tests/run_interactive_regression.py $(PROJECT).img

release-check: tests/run_release_candidate_check.sh
	tests/run_release_candidate_check.sh

debug: $(PROJECT).iso
	qemu-system-x86_64 -machine q35 -m 256M -cdrom $(PROJECT).iso -boot d \
		-serial stdio -display none -no-reboot -no-shutdown -S -s

inspect: $(KERNEL)
	readelf -h -l -S $(KERNEL)

clean:
	rm -rf $(BUILD_DIR) $(PROJECT).iso $(PROJECT).img

distclean: clean
	rm -rf $(LIMINE_DIR)

help:
	@printf '%s\n' \
		'make                Build a hybrid BIOS/UEFI ISO image.' \
		'make run            Start BIOS QEMU headlessly and show COM1 output.' \
		'make run-graphic    Start BIOS QEMU with framebuffer window and COM1 output.' \
		'make run-uefi       Start UEFI QEMU headlessly and show COM1 output.' \
		'make run-uefi-graphic Start UEFI QEMU with framebuffer window and COM1 output.' \
		'make smoke          Run headless BIOS and UEFI boot smoke checks against myos.img.' \
		'make regression     Run isolated GUI, persistent-storage and native-workflow BIOS/UEFI regression.' \
		'make release-check  Clean rebuild, artifact hashes, BIOS/UEFI smoke and interactive regression; creates no tag.' \
		'make img            Build a raw hybrid GPT disk/USB image. Flash only to a dedicated test device.' \
		'make debug          Start QEMU paused with a GDB server on TCP port 1234.'

-include $(OBJECTS:.o=.d)
