mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
BCB_ROOT := $(abspath $(dir $(mkfile_path)))
ZERO_APP_DIR := $(BCB_ROOT)/apps/zero
MCUBOOT_DIR := $(BCB_ROOT)/zephyr-os/bootloader/mcuboot/boot/zephyr
MCUBOOT_KEY := $(BCB_ROOT)/zephyr-os/bootloader/mcuboot/root-rsa-2048.pem
DTS_ROOT := $(BCB_ROOT)
ZEPHYR_MODULE_BCB := $(BCB_ROOT)/modules/bcb
OVERLAY_CONFIG := $(BCB_ROOT)/boards/arm/bcb_v1/bcb_v1_overlay_mcuboot.conf 

BUILD_DIR = build/zephyr
ARTIFACTS = build


basics: 
	west update
	pip install -r scripts/requirements.txt

build_dir:
	mkdir -p $(ARTIFACTS)

mcuboot: build_dir
	cd $(MCUBOOT_DIR) && west build -b bcb_v1 -- -DBOARD_ROOT=$(BCB_ROOT) -DDTS_ROOT=$(DTS_ROOT) -DZEPHYR_EXTRA_MODULES=$(ZEPHYR_MODULE_BCB) -DOVERLAY_CONFIG=$(OVERLAY_CONFIG)
	cp $(MCUBOOT_DIR)/$(BUILD_DIR)/zephyr.bin $(ARTIFACTS)/mcuboot.bin

mcuboot-flash: mcuboot
	cd $(MCUBOOT_DIR) && west flash

zero: build_dir
	cd $(ZERO_APP_DIR) && west build && \
	west sign -t imgtool -- --key $(MCUBOOT_KEY)
	cp $(ZERO_APP_DIR)/$(BUILD_DIR)/zephyr.signed.bin $(ARTIFACTS)/zero.signed.bin

zero-flash: zero
	cd $(ZERO_APP_DIR) && west flash --bin-file $(ZERO_APP_DIR)/$(BUILD_DIR)/zephyr.signed.bin

clean:
	rm -rf build $(MCUBOOT_DIR)/build apps/zero/build

all: basics mcuboot zero


