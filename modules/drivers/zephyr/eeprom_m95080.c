#define DT_DRV_COMPAT st_m95080


#include <drivers/eeprom.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_EEPROM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(eeprom_m95080);

struct eeprom_m95080_config {
	const char *spi_bus;
	struct spi_config spi_cfg;
	const char *cs_gpio;
	int cs_pin;
};

struct eeprom_m95080_data {
    struct device *spi;
    struct spi_cs_control spi_cs;
    struct k_sem lock;
};

static int eeprom_m95080_read(struct device *dev, off_t offset, void *buf, size_t len)
{
    return 0;
}

static int eeprom_m95080_write(struct device *dev, off_t offset, const void *buf, size_t len)
{
    return 0;
}

static size_t eeprom_m95080_size(struct device *dev)
{

    return 0;
}

static int eeprom_m95080_init(struct device* dev)
{
    return 0;
}

static const struct eeprom_driver_api eeprom_m95080_api = {
    .read = eeprom_m95080_read,
    .write = eeprom_m95080_write,
    .size = eeprom_m95080_size,
};

#define EEPROM_M95080_DEVICE(n)                                                                     \
    static struct eeprom_m95080_data eeprom_m95080_##n##_data = {                                   \
    };                                                                                              \
    static struct eeprom_m95080_config eeprom_m95080_##n##_config = {                               \
        .spi_bus = DT_INST_BUS_LABEL(n),                                                            \
        .spi_cfg = {                                                                                \
            .frequency = DT_INST_PROP(n, spi_max_frequency),                                        \
            .operation = SPI_OP_MODE_MASTER,                                                        \
            .slave = DT_INST_REG_ADDR(n),                                                           \
            .cs = &eeprom_m95080_##n##_data.spi_cs,                                                 \
        },                                                                                          \
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(n), (		                                        \
			.cs_gpio = DT_INST_SPI_DEV_CS_GPIOS_LABEL(n),                                           \
			.cs_pin  = DT_INST_SPI_DEV_CS_GPIOS_PIN(n),))	                                        \
    };                                                                                              \
    DEVICE_AND_API_INIT(eeprom_m95080_##n,                                                          \
                        DT_INST_LABEL(n),                                                           \
                        &eeprom_m95080_init,                                                        \
                        &eeprom_m95080_##n##_data,                                                  \
                        &eeprom_m95080_##n##_config,                                                \
                        POST_KERNEL,                                                                \
                        CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                         \
                        &eeprom_m95080_api);                                                        \

DT_INST_FOREACH_STATUS_OKAY(EEPROM_M95080_DEVICE)