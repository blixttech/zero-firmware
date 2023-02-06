MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SOURCE_ROOT_DIR := $(abspath $(dir $(MAKEFILE_PATH)))

ZEPHYR_BASE := $(SOURCE_ROOT_DIR)/zephyr
PLATFORM_MODULE_DIR := $(SOURCE_ROOT_DIR)/modules/zero/platform
BOARD_ROOT_DIR := $(PLATFORM_MODULE_DIR)
DTS_ROOT_DIR := $(PLATFORM_MODULE_DIR)

# This variable contains paths to zephyr modules in this repository.
REPO_MODULES_DIRS := $(PLATFORM_MODULE_DIR)

SYSCALL_INCLUDE_DIRS := $(PLATFORM_MODULE_DIR)/include

APP_VERSION := $(shell git name-rev --tags --name-only \
                $$(git rev-parse HEAD 2>/dev/null) 2>/dev/null | \
                sed 's/v//g;s/\^.*//g;s/undefined/0.0.0/g;')

ifeq ($(strip $(APP_VERSION)),)
    APP_VERSION := "0.0.0"
endif

APP_KEY_PAIR := $(SOURCE_ROOT_DIR)/modules/bootloader/mcuboot/root-rsa-2048.pem

include $(SOURCE_ROOT_DIR)/scripts/makefiles/identify-target.mk

app_makefile := $(SOURCE_ROOT_DIR)/scripts/makefiles/apps/$(APP).mk
ifeq ($(wildcard $(app_makefile)),)
    $(error Cannot find application makefile)
endif

include $(app_makefile)

board_makefile := $(SOURCE_ROOT_DIR)/scripts/makefiles/boards/$(BOARD).mk
ifeq ($(wildcard $(board_makefile)),)
    $(error Cannot find board makefile)
endif

include $(board_makefile)

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

WEST = west

CMAKE_ARGS += -DZEPHYR_BASE=$(ZEPHYR_BASE)
CMAKE_ARGS += -DBOARD_ROOT=$(BOARD_ROOT_DIR)
CMAKE_ARGS += -DDTS_ROOT=$(DTS_ROOT_DIR)
CMAKE_ARGS += -DCONFIG_STACK_USAGE=y
CMAKE_ARGS += -DSYSCALL_INCLUDE_DIRS=$(subst $(SPACE),;,$(SYSCALL_INCLUDE_DIRS))
CMAKE_ARGS += -DZEPHYR_EXTRA_MODULES=$(subst $(SPACE),;,$(REPO_MODULES_DIRS))

ifneq ($(APP_OVERLAY_CONFIG),)
    CMAKE_ARGS += -DOVERLAY_CONFIG=$(APP_OVERLAY_CONFIG)
endif

CMAKE_ARGS += -DCMAKE_EXPORT_COMPILE_COMMANDS=1


.PHONY: savetarget app flash mem-usage clean
.DEFAULT_GOAL := app

all: app

app:
	cd $(APP_DIR) && $(WEST) build -b $(BOARD) -- $(CMAKE_ARGS)
ifeq ($(APP_SIGNED),1)
	cd $(APP_DIR) && $(WEST) sign -t imgtool -- --key $(APP_KEY_PAIR) --version $(APP_VERSION)
endif

mem-usage: app
	cd $(APP_DIR) && $(WEST) build -t puncover 

flash: app
	cd $(APP_DIR) && $(WEST) flash $(APP_HEX)

clean:
	rm -rf $(APP_BUILD_DIR)
