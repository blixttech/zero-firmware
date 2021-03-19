MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SOURCE_ROOT_DIR := $(abspath $(dir $(MAKEFILE_PATH)))
BINARY_DIR := $(SOURCE_ROOT_DIR)/build

# TODO: Create an overlay config file to set the key pair for MCUBoot bootloader
#       and Zero application 
KEY_PAIR := $(SOURCE_ROOT_DIR)/zephyr-os/bootloader/mcuboot/root-rsa-2048.pem

# Bootloader variables
MCUBOOT_APP_DIR := $(SOURCE_ROOT_DIR)/zephyr-os/bootloader/mcuboot/boot/zephyr
MCUBOOT_BOARD_ROOT := $(SOURCE_ROOT_DIR)
MCUBOOT_DTS_ROOT := $(SOURCE_ROOT_DIR)
MCUBOOT_EXTRA_MODULES := $(SOURCE_ROOT_DIR)/modules/bcb
MCUBOOT_OVERLAY_CONFIG := $(SOURCE_ROOT_DIR)/boards/arm/bcb_v1/bcb_v1_overlay_mcuboot.conf 
MCUBOOT_BUILD_DIR := $(MCUBOOT_APP_DIR)/build
MCUBOOT_BIN := $(MCUBOOT_BUILD_DIR)/zephyr/zephyr.bin

# Zero variables
ZERO_APP_DIR := $(SOURCE_ROOT_DIR)/apps/zero
ZERO_BUILD_DIR := $(ZERO_APP_DIR)/build
ZERO_BIN := $(ZERO_BUILD_DIR)/zephyr/zephyr.signed.bin

VERSION := $(shell git name-rev --tags --name-only $$(git rev-parse HEAD) | \
           sed 's/v//g;s/\^.*//g;s/undefined/0.0.0/g')

.PHONY: all mcuboot mcuboot-flash zero zero-flash binaries clean

all: zero

init: 
	west update
	pip install -r scripts/requirements.txt

# MCUboot rules
ifeq ($(INCREMENTAL),1)
mcuboot:
else
mcuboot: $(MCUBOOT_BIN)
$(MCUBOOT_BIN):
endif
	cd $(MCUBOOT_APP_DIR) && west build -b bcb_v1 -- -DBOARD_ROOT=$(MCUBOOT_BOARD_ROOT) \
	-DDTS_ROOT=$(MCUBOOT_DTS_ROOT) -DZEPHYR_EXTRA_MODULES=$(MCUBOOT_EXTRA_MODULES) \
	-DOVERLAY_CONFIG=$(MCUBOOT_OVERLAY_CONFIG) -DCMAKE_EXPORT_COMPILE_COMMANDS=1

ifeq ($(INCREMENTAL),1)
mcuboot-flash: mcuboot
else
mcuboot-flash: $(MCUBOOT_BIN)
endif
	cd $(MCUBOOT_APP_DIR) && west flash --bin-file $(MCUBOOT_BIN)

# Zero app rules
ifeq ($(INCREMENTAL),1)
zero:
else
zero: $(ZERO_BIN)
$(ZERO_BIN):
endif
	cd $(ZERO_APP_DIR) && west build -- -DCMAKE_EXPORT_COMPILE_COMMANDS=1 && \
	west sign -t imgtool -- --key $(KEY_PAIR) --version $(VERSION)

ifeq ($(INCREMENTAL),1)
zero-flash: zero
else
zero-flash: $(ZERO_BIN)
endif
	cd $(ZERO_APP_DIR) && west flash --bin-file $(ZERO_BIN)

$(BINARY_DIR):
	mkdir -p $@

binaries: $(BINARY_DIR) $(MCUBOOT_BIN) $(ZERO_BIN)
	cp $(MCUBOOT_BIN) $(BINARY_DIR)/mcuboot-$(VERSION).bin && \
	cp $(ZERO_BIN) $(BINARY_DIR)/zero-$(VERSION).signed.bin

clean:
	rm -rf $(BINARY_DIR) $(MCUBOOT_BUILD_DIR) $(ZERO_BUILD_DIR)

