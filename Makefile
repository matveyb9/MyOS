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

.PHONY: all kernel initramfs iso img run run-graphic run-uefi run-uefi-graphic debug clean distclean inspect help

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

$(INITRAMFS): $(USER_INIT) $(USER_HELLO) $(USER_SLEEPER) $(USER_ORPHANER) $(USER_SAFETY) $(USER_MOTD) tools/mkcpio.py
	python3 tools/mkcpio.py $@ init $(USER_INIT) hello $(USER_HELLO) sleeper $(USER_SLEEPER) orphaner $(USER_ORPHANER) safety $(USER_SAFETY) motd.txt $(USER_MOTD)

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
	dd if=/dev/zero of=$@ bs=1M count=64 status=none
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $@ -n 1:2048 -t 1:ef00 -m 1
	$(LIMINE_DIR)/limine bios-install $@
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
		'make img            Build a raw hybrid GPT disk/USB image. Flash only to a dedicated test device.' \
		'make debug          Start QEMU paused with a GDB server on TCP port 1234.'

-include $(OBJECTS:.o=.d)
