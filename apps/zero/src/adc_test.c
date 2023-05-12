#include "adc_test.h"

#include <zephyr/drivers/adc_dma.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc_test);

#define Z_ADC_DEV(inst)	    (adc_test_data.dev_adc_##inst)
#define Z_ADC_SEQ_IDX(inst) (adc_test_data.seq_idx_adc_##inst)

#define Z_ADC_DEV_LABEL(node_label, ch_name)                                                       \
	DEVICE_DT_NAME(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name))

#define Z_ADC_CHANNEL(node_label, ch_name)                                                         \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, main)

#define Z_ADC_CHANNEL_ALT(node_label, ch_name)                                                     \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, alt)

#define Z_ADC_CHANNEL_TYPE(node_label, ch_name)                                                    \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, type)

#define Z_ADC_SEQ_ADD(inst, node_label, ch_name)                                                   \
	do {                                                                                       \
		struct adc_dma_channel_config config;                                              \
		config.channel = Z_ADC_CHANNEL(node_label, ch_name);                               \
		config.alt_channel = Z_ADC_CHANNEL_ALT(node_label, ch_name);                       \
		config.type = Z_ADC_CHANNEL_TYPE(node_label, ch_name);                             \
		const struct device *dev =                                                         \
			device_get_binding(Z_ADC_DEV_LABEL(node_label, ch_name));                  \
		if (dev == Z_ADC_DEV(inst)) {                                                      \
			int r;                                                                     \
			r = adc_dma_add_channel(dev, &config, Z_ADC_SEQ_IDX(inst)++);              \
			if (r) {                                                                   \
				LOG_ERR("cannot setup adc##inst channel: %" PRIu8 ", %" PRIu8,     \
					config.channel, config.alt_channel);                       \
				return r;                                                          \
			}                                                                          \
		} else {                                                                           \
			LOG_ERR("invalid adc device");                                             \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

#define Z_ADC_INIT(inst)                                                                           \
	do {                                                                                       \
		Z_ADC_DEV(inst) = device_get_binding(DEVICE_DT_NAME(DT_NODELABEL(adc##inst)));     \
		if (Z_ADC_DEV(inst) == NULL) {                                                     \
			LOG_ERR("cannot get adc device");                                          \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0);

#define MAX_SAMPLES 150

typedef struct adc_test_data {
	const struct device *dev_adc_0;
	uint8_t seq_idx_adc_0;

	uint16_t buf_adc_0[MAX_SAMPLES] __attribute__((aligned(2)));
} adc_test_data_t;

static adc_test_data_t adc_test_data;

void adc_completed(const struct device *dev, void *buffer, size_t buffer_size)
{
	LOG_INF("buffer 0x%" PRIx32 ", size %" PRId32, (uint32_t)buffer, buffer_size);
}

int adc_test_init(void)
{
	struct adc_dma_sampling_config sampling_config;
	int r;

	Z_ADC_INIT(0);
	Z_ADC_SEQ_ADD(0, aread, i_low_gain);
	Z_ADC_SEQ_ADD(0, aread, v_mains);

	sampling_config.buffer = adc_test_data.buf_adc_0;
	sampling_config.buffer_size = sizeof(adc_test_data.buf_adc_0);
	sampling_config.sequence_length = adc_test_data.seq_idx_adc_0;
	sampling_config.samples_per_channel = MAX_SAMPLES / adc_test_data.seq_idx_adc_0;
	sampling_config.reference = ADC_DMA_REF_EXTERNAL0;
	sampling_config.resolution = 16;
	sampling_config.mode = ADC_DMA_SAMPLING_MODE_CONTINUOUS;
	sampling_config.sample_interval = 1000;
	sampling_config.callback = &adc_completed;

	r = adc_dma_start(Z_ADC_DEV(0), &sampling_config);
	if (r) {
		LOG_ERR("cannot start adc: %d", r);
	}

	k_sleep(K_MSEC(1000));

	adc_dma_stop(Z_ADC_DEV(0));

	return 0;
}