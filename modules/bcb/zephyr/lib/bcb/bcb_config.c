#include "bcb_config.h"
#include <init.h>
#include <drivers/eeprom.h>
#include <sys/crc.h>

#define LOG_LEVEL CONFIG_BCB_CONFIG_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bcb_config);

#define BCB_CONFIG_EEPROM_LABEL		DT_LABEL(DT_CHOSEN(breaker_config_eeprom))
#define BCB_CONFIG_MAGIC		0xabcdU

struct bcb_config_data {
	struct device *dev_eeprom;
};

struct __attribute__((packed)) config_header {
	uint16_t magic;
	uint16_t size;
	uint16_t crc;
};

static struct bcb_config_data config_data;

int bcb_config_load(off_t offset, uint8_t *data, size_t size)
{
	int r;
	struct config_header header;

	if (!config_data.dev_eeprom) {
		LOG_ERR("Could not get EEPROM device");
		return -ENOENT;
	}

	r = eeprom_read(config_data.dev_eeprom, offset, &header, sizeof(header));
	if (r) {
		LOG_ERR("Cannot read EEPROM: %d", r);
		return r;
	}

	if (header.magic != BCB_CONFIG_MAGIC) {
		LOG_ERR("Invalid magic: 0x%04x", header.magic);
		return -EINVAL;
	}

	if (size != header.size) {
		LOG_ERR("Invalid size: %d", header.size);
		return -EINVAL;
	}

	offset += sizeof(header);
	r = eeprom_read(config_data.dev_eeprom, offset, data, size);
	if (r) {
		LOG_ERR("Cannot read EEPROM: %d", r);
		return r;
	}

	if (header.crc != crc16_ccitt(0, data, size)) {
		LOG_ERR("Invalid CRC");
		return -EINVAL;
	}

	return 0;
}

int bcb_config_store(off_t offset, uint8_t *data, size_t size)
{
	int r;
	struct config_header header;

	if (!config_data.dev_eeprom) {
		LOG_ERR("Could not get EEPROM device");
		return -ENOENT;
	}

	header.magic = BCB_CONFIG_MAGIC;
	header.size = size;
	header.crc = crc16_ccitt(0, data, size);

	r = eeprom_write(config_data.dev_eeprom, offset, &header, sizeof(header));
	if (r) {
		LOG_ERR("Cannot write EEPROM: %d", r);
		return r;
	}

	offset += sizeof(header);
	r = eeprom_write(config_data.dev_eeprom, offset, data, size);
	if (r) {
		LOG_ERR("Cannot write EEPROM: %d", r);
		return r;
	}

	return 0;
}

int bcb_config_init(void)
{
	config_data.dev_eeprom = device_get_binding(BCB_CONFIG_EEPROM_LABEL);
	if (!config_data.dev_eeprom) {
		LOG_ERR("Could not get EEPROM device");
		return -ENOENT;
	}

	return 0;
}