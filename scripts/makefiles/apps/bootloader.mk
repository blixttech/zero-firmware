APP_DIR := $(SOURCE_ROOT_DIR)/modules/bootloader/mcuboot/boot/zephyr
APP_BUILD_DIR := $(APP_DIR)/build
APP_OVERLAY_CONFIG := $(PLATFORM_MODULE_DIR)/boards/arm/zero/zero_mcuboot_overlay.conf
APP_BIN := $(APP_BUILD_DIR)/zephyr/zephyr.bin
APP_HEX := $(APP_BUILD_DIR)/zephyr/zephyr.hex
APP_SIGNED = 0
