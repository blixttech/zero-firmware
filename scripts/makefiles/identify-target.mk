ifneq ($(APP), $(strip $(APP)))
    $(error App name '$(APP)' contains trailing whitespace)
endif

ifneq ($(BOARD), $(strip $(BOARD)))
    $(error Board name '$(BOARD)' contains trailing whitespace)
endif

app_specified := $(APP)
board_specified := $(BOARD)

saved_target_file := $(SOURCE_ROOT_DIR)/saved-target.mk
ifneq ($(wildcard $(saved_target_file)),)
    include $(saved_target_file)
endif

ifneq ($(app_specified),)
    APP := $(app_specified)
endif

ifneq ($(board_specified),)
    BOARD := $(board_specified)
endif

ifeq ($(APP),)
    $(error APP not specified)
endif

ifeq ($(BOARD),)
    $(error BOARD not specified)
endif

savetarget:
	@rm -f $(saved_target_file)
	@echo "Saving target"
	@echo >$(saved_target_file) "APP = $(APP)"
	@echo >>$(saved_target_file) "BOARD = $(BOARD)"

