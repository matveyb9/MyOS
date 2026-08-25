SDK_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PROJECT_ROOT := $(abspath $(SDK_ROOT)/..)

APP ?= examples/hello.c
OUT ?= build/hello.elf

CC ?= gcc
LD ?= ld

ifneq ($(filter /%,$(APP)),)
APP_PATH := $(APP)
else ifneq ($(wildcard $(APP)),)
APP_PATH := $(abspath $(APP))
else
APP_PATH := $(PROJECT_ROOT)/$(APP)
endif

ifneq ($(filter /%,$(OUT)),)
OUT_PATH := $(OUT)
else ifneq ($(filter sdk/%,$(OUT)),)
OUT_PATH := $(PROJECT_ROOT)/$(OUT)
else
OUT_PATH := $(SDK_ROOT)/$(OUT)
endif

WARNINGS := -Wall -Wextra -Werror -Wshadow -Wconversion -Wundef
CFLAGS := -std=gnu11 -O0 -g $(WARNINGS) \
          -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
          -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mcmodel=small \
          -mno-mmx -mno-sse -mno-sse2 -I$(SDK_ROOT)/include
LDFLAGS := -m elf_x86_64 -nostdlib -static -T $(SDK_ROOT)/myos-user.ld

APP_OBJECT := $(dir $(OUT_PATH))app.o
CRT_OBJECT := $(dir $(OUT_PATH))crt0.o

.PHONY: all clean inspect

all: $(OUT_PATH)

$(OUT_PATH): $(APP_PATH) $(SDK_ROOT)/lib/crt0.c $(SDK_ROOT)/include/myos.h $(SDK_ROOT)/myos-user.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(APP_PATH) -o $(APP_OBJECT)
	$(CC) $(CFLAGS) -c $(SDK_ROOT)/lib/crt0.c -o $(CRT_OBJECT)
	$(LD) $(LDFLAGS) $(CRT_OBJECT) $(APP_OBJECT) -o $@

clean:
	rm -rf $(SDK_ROOT)/build

inspect: $(OUT_PATH)
	readelf -h -l $(OUT_PATH)
