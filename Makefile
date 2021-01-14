MCUBOOT_DIR=zephyr-os/bootloader/mcuboot/boot/zephyr/
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
BCB_ROOT :=$(dir $(mkfile_path))
DTS_ROOT :=$(BCB_ROOT)
ZEPHYR_MODULE_BCB :=$(BCB_ROOT)/modules/bcb
OVERLAY_CONFIG :=$(BCB_ROOT)/boards/arm/bcb_v1/bcb_v1_overlay_mcuboot.conf 

BUILD_DIR=build/zephyr/
ARTIFACTS=build


basics: 
	west update
	pip install -r scripts/requirements.txt

build_dir:
	mkdir -p $(ARTIFACTS)

mcuboot: build_dir
	cd $(MCUBOOT_DIR) && west build -b bcb_v1 -- -DBOARD_ROOT=$(BCB_ROOT) -DDTS_ROOT=$(DTS_ROOT) -DZEPHYR_EXTRA_MODULES=$(ZEPHYR_MODULE_BCB) -DOVERLAY_CONFIG=$(OVERLAY_CONFIG)
	cp $(MCUBOOT_DIR)$(BUILD_DIR)zephyr.bin $(ARTIFACTS)/mcuboot.bin

zero: build_dir
	cd apps/zero && west build && \
	west sign -t imgtool -- --key ../../zephyr-os/bootloader/mcuboot/root-rsa-2048.pem
	cp apps/zero/$(BUILD_DIR)/zephyr.signed.bin $(ARTIFACTS)/zero.signed.bin

clean:
	rm -rf build $(MCUBOOT_DIR)/build apps/zero/build

all: basics mcuboot zero


