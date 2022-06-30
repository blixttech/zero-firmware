#include "adc_perf.h"
#include <lib/bcb_macros.h>
#include <lib/bcb_msmnt.h>
#include <drivers/adc_dma.h>
#include <drivers/adc_trigger.h>
#include <device.h>
#include <devicetree.h>
#include <sys/_stdint.h>
#include <sys/util.h>
#include <fcntl.h>
#include <net/socket.h>
#include <net/net_ip.h>
#include <net/buf.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <tinycbor/cbor.h>
#include <arm_math.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_perf);

// clang-format off
#define BCB_ADC_DEV(inst)	(adc_perf_data.dev_adc_##inst)
#define BCB_ADC_SEQ_IDX(inst)	(adc_perf_data.seq_idx_adc_##inst)
#define BCB_ADC_SEQ_A		(adc_perf_data.a_b_data[adc_perf_data.seq_idx_adc_0].a)
#define BCB_ADC_SEQ_B		(adc_perf_data.a_b_data[adc_perf_data.seq_idx_adc_0].b)
// clang-format on

#define BCB_ADC_DEV_LABEL(node_label, ch_name)                                                     \
	DT_LABEL(DT_PHANDLE_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name))

#define BCB_ADC_CHANNEL(node_label, ch_name)                                                       \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, input)

#define BCB_ADC_CHANNEL_ALT(node_label, ch_name)                                                   \
	DT_PHA_BY_NAME(DT_NODELABEL(node_label), io_channels, ch_name, muxsel)

#define BCB_ADC_INIT(inst)                                                                         \
	do {                                                                                       \
		BCB_ADC_DEV(inst) = device_get_binding(DT_LABEL(DT_NODELABEL(adc##inst)));         \
		if (BCB_ADC_DEV(inst) == NULL) {                                                   \
			LOG_ERR("Cannot get ADC device");                                          \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0);

#define BCB_ADC_SEQ_ADD(inst, node_label, ch_name)                                                 \
	do {                                                                                       \
		struct adc_dma_channel_config config;                                              \
		config.channel = BCB_ADC_CHANNEL(node_label, ch_name);                             \
		config.alt_channel = BCB_ADC_CHANNEL_ALT(node_label, ch_name);                     \
		struct device *dev = device_get_binding(BCB_ADC_DEV_LABEL(node_label, ch_name));   \
		if (dev == BCB_ADC_DEV(inst)) {                                                    \
			int r;                                                                     \
			r = adc_dma_channel_setup(dev, BCB_ADC_SEQ_IDX(inst)++, &config);          \
			if (r) {                                                                   \
				LOG_ERR("Cannot setup ADC##inst channel: %" PRIu8 ", %" PRIu8,     \
					config.channel, config.alt_channel);                       \
				return r;                                                          \
			}                                                                          \
		} else {                                                                           \
			LOG_ERR("Invalid ADC device");                                             \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

#define BCB_ADC_SEQ_ADD_A_B(type)                                                                  \
	do {                                                                                       \
		uint16_t val;                                                                      \
		bcb_msmnt_get_calib_param_a(type, &val);                                           \
		BCB_ADC_SEQ_A = val;                                                               \
		bcb_msmnt_get_calib_param_b(type, &val);                                           \
		BCB_ADC_SEQ_B = val;                                                               \
	} while (0)

// clang-format off
#define SERVER_PORT		5555
#define MAX_CLIENTS		4
#define RX_BUF_SIZE		128
#define CBOR_BUF_SIZE		2048
#define MAX_SEQ_SAMPLES		510
#define THREAD_STACK_SIZE	1024
#define THREAD_PRIORITY		K_PRIO_COOP(8)
#define MAX_CHANNELS		3
// clang-format on

struct client_info {
	bool used;
	struct sockaddr addr;
	uint32_t last_seen;
};

struct seq_a_b_data {
	uint16_t a;
	uint16_t b;
};

typedef struct adc_perf_data {
	struct device *dev_adc_0;
	uint8_t seq_idx_adc_0;
	uint32_t sample_size_adc_0;
	uint32_t sample_time_adc_0;
	uint32_t sample_interval_adc_0;

	uint32_t sample_block_seq;
	int udp_socket;
	struct k_work udp_tx_work;
	struct k_fifo udp_tx_fifo;
	struct k_work udp_rx_work;
	struct k_fifo udp_rx_fifo;
	struct k_delayed_work client_recycle_work;
	struct client_info clients[MAX_CLIENTS];
	uint8_t recv_buf[RX_BUF_SIZE];
	uint8_t cbor_buf[CBOR_BUF_SIZE];
	uint64_t sqr_sum[MAX_CHANNELS];
	uint32_t sqr_sum_samples;
	struct seq_a_b_data a_b_data[MAX_CHANNELS];
} adc_perf_data_t;

static adc_perf_data_t adc_perf_data;

static uint16_t buf_adc_0[MAX_SEQ_SAMPLES] __attribute__((aligned(2)));

NET_BUF_POOL_DEFINE(tx_buf_pool, 4, MAX_SEQ_SAMPLES * sizeof(uint16_t), sizeof(uint64_t), NULL);
NET_BUF_POOL_DEFINE(rx_buf_pool, 4, sizeof(adc_perf_data.recv_buf), sizeof(struct sockaddr), NULL);

static void process_udp(void);

K_THREAD_DEFINE(udp_thread, THREAD_STACK_SIZE, process_udp, NULL, NULL, NULL, THREAD_PRIORITY, 0,
		-1);

static void on_seq_done(struct device *dev, volatile void *buffer, uint32_t samples)
{
	struct net_buf *buf;

	adc_perf_data.sample_block_seq++;

	buf = net_buf_alloc(&tx_buf_pool, K_NO_WAIT);
	if (!buf) {
		/* If there is no space in the pool, we remove the oldest sample. */
		buf = net_buf_get(&adc_perf_data.udp_tx_fifo, K_NO_WAIT);
		if (!buf) {
			return;
		}
	}

	net_buf_reset(buf);
	net_buf_add_mem(buf, (void *)buffer, samples * sizeof(uint16_t));
	/* user data buffer is aligned to void * */
	*((uint32_t *)net_buf_user_data(buf)) = adc_perf_data.sample_block_seq;

	net_buf_put(&adc_perf_data.udp_tx_fifo, buf);
	k_work_submit(&adc_perf_data.udp_tx_work);
}

static int cbor_write(struct cbor_encoder_writer *writer, const char *data, int len)
{
	if (writer->bytes_written + len > sizeof(adc_perf_data.cbor_buf)) {
		return CborErrorOutOfMemory;
	}

	memcpy(&adc_perf_data.cbor_buf[writer->bytes_written], data, len);
	writer->bytes_written += len;

	return CborNoError;
}

static inline uint32_t create_cbor_frame(struct net_buf *buf)
{
	uint32_t seq;
	CborEncoder encoder;
	CborEncoder array_encoder;
	cbor_encoder_writer writer = {
		.write = cbor_write,
		.bytes_written = 0,
	};

	cbor_encoder_init(&encoder, &writer, 0);
	cbor_encoder_create_array(&encoder, &array_encoder, CborIndefiniteLength);

	/* user data buffer is aligned to void * */
	seq = *((uint32_t *)net_buf_user_data(buf));
	cbor_encode_uint(&array_encoder, seq);
	cbor_encode_uint(&array_encoder, adc_perf_data.sample_interval_adc_0);
	cbor_encode_uint(&array_encoder, adc_perf_data.sample_time_adc_0);
	cbor_encode_uint(&array_encoder, adc_perf_data.seq_idx_adc_0);
	cbor_encode_uint(&array_encoder, sizeof(uint16_t) * 8);
	cbor_encode_byte_string(&array_encoder, (uint8_t *)adc_perf_data.a_b_data,
				adc_perf_data.seq_idx_adc_0 * sizeof(struct seq_a_b_data));
	cbor_encode_byte_string(&array_encoder, (uint8_t *)buf->data, buf->len);

	cbor_encoder_close_container(&encoder, &array_encoder);

	return writer.bytes_written;
}

static inline void send_to_clients(struct net_buf *buf)
{
	int i;
	uint32_t bytes_written = create_cbor_frame(buf);

	for (i = 0; i < MAX_CLIENTS; i++) {
		int r;

		if (!adc_perf_data.clients[i].used) {
			continue;
		}

		r = sendto(adc_perf_data.udp_socket, adc_perf_data.cbor_buf, bytes_written, 0,
			   &adc_perf_data.clients[i].addr, sizeof(struct sockaddr));
		if (r < 0) {
			LOG_ERR("send error %d", r);
		}
	}
}

static inline void calculate_sqr_sum(struct net_buf *buf)
{
	int i;
	int c;
	int total_samples;
	int samples_per_channel;

	if (!adc_perf_data.seq_idx_adc_0 || adc_perf_data.seq_idx_adc_0 > MAX_CHANNELS) {
		/* Sanity check. Incorrect number of ADC channels */
		return;
	}

	if (buf->len % (sizeof(uint16_t) * adc_perf_data.seq_idx_adc_0)) {
		/* Sanity check. Invalid buffer */
		return;
	}

	total_samples = buf->len / sizeof(uint16_t);
	samples_per_channel /= adc_perf_data.seq_idx_adc_0;

	for (i = 0; i < total_samples; i += adc_perf_data.seq_idx_adc_0) {
		for (c = 0; c < adc_perf_data.seq_idx_adc_0; c++) {
			uint16_t *data = ((uint16_t *)buf->data) + i + c;
			adc_perf_data.sqr_sum[c] += ((uint32_t)(*data) * (uint32_t)(*data));
		}
	}

	adc_perf_data.sqr_sum_samples = samples_per_channel;
}

static void udp_tx_work(struct k_work *work)
{
	struct net_buf *buf;

	while ((buf = net_buf_get(&adc_perf_data.udp_tx_fifo, K_NO_WAIT)) != NULL) {
		//calculate_sqr_sum(buf);
		send_to_clients(buf);
		net_buf_unref(buf);
	}
}

static bool is_sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b)
{
	/* FIXME: Should we consider ipv6-mapped ipv4 addresses as equal to  ipv4 addresses? */
	if (a->sa_family != b->sa_family) {
		return false;
	}

	if (a->sa_family == AF_INET) {
		const struct sockaddr_in *a4 = net_sin(a);
		const struct sockaddr_in *b4 = net_sin(b);
		if (a4->sin_port != b4->sin_port) {
			return false;
		}
		return net_ipv4_addr_cmp(&a4->sin_addr, &b4->sin_addr);
	}

	if (b->sa_family == AF_INET6) {
		const struct sockaddr_in6 *a6 = net_sin6(a);
		const struct sockaddr_in6 *b6 = net_sin6(b);
		if (a6->sin6_port != b6->sin6_port) {
			return false;
		}
		return net_ipv6_addr_cmp(&a6->sin6_addr, &b6->sin6_addr);
	}
	/* Invalid address family */
	return false;
}

static inline struct client_info *find_client(struct sockaddr *addr)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!adc_perf_data.clients[i].used) {
			continue;
		}

		if (is_sockaddr_equal(addr, &adc_perf_data.clients[i].addr)) {
			return &adc_perf_data.clients[i];
		}
	}

	return NULL;
}

static inline struct client_info *add_client(struct sockaddr *addr)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!adc_perf_data.clients[i].used) {
			adc_perf_data.clients[i].used = true;
			adc_perf_data.clients[i].last_seen = k_uptime_get_32();
			memcpy(&adc_perf_data.clients[i].addr, addr, sizeof(struct sockaddr));
			return &adc_perf_data.clients[i];
		}
	}

	return NULL;
}

static void client_recycle_work(struct k_work *work)
{
	int i;
	uint32_t now = k_uptime_get_32();

	for (i = 0; i < MAX_CLIENTS; i++) {
		uint32_t diff;

		if (!adc_perf_data.clients[i].used) {
			continue;
		}

		diff = now > adc_perf_data.clients[i].last_seen ?
				     now - adc_perf_data.clients[i].last_seen :
				     (UINT32_MAX - adc_perf_data.clients[i].last_seen) + now;

		if (diff > 3000) {
			adc_perf_data.clients[i].used = false;
			LOG_INF("removed unresponstive client");
		}
	}

	k_delayed_work_submit(&adc_perf_data.client_recycle_work, K_MSEC(1000));
}

static void udp_rx_work(struct k_work *work)
{
	struct net_buf *buf;

	while ((buf = net_buf_get(&adc_perf_data.udp_rx_fifo, K_NO_WAIT)) != NULL) {
		struct sockaddr *addr = (struct sockaddr *)net_buf_user_data(buf);
		struct client_info *client;

		client = find_client(addr);
		if (!client) {
			if (!add_client(addr)) {
				LOG_ERR("cannot add client");
			}
		} else {
			client->last_seen = k_uptime_get_32();
		}

		net_buf_unref(buf);
	}
}

static inline uint32_t get_even_max_div(uint32_t max, uint32_t base)
{
	if (!max || !base) {
		return 0;
	}

	max = (max / base) * base;
	do {
		if (max % 2 == 0) {
			return max;
		}
		max -= base;
	} while (max > 0);

	return 0;
}

static int start_adc(void)
{
	int r;
	adc_dma_sequence_config_t seq_cfg;
	struct device *trigger_dev;

	adc_perf_data.seq_idx_adc_0 = 0;

	BCB_ADC_SEQ_ADD_A_B(BCB_MSMNT_TYPE_I_LOW_GAIN);
	BCB_ADC_SEQ_ADD(0, aread, i_low_gain);

	BCB_ADC_SEQ_ADD_A_B(BCB_MSMNT_TYPE_I_HIGH_GAIN);
	BCB_ADC_SEQ_ADD(0, aread, i_high_gain);

	BCB_ADC_SEQ_ADD_A_B(BCB_MSMNT_TYPE_V_MAINS);
	BCB_ADC_SEQ_ADD(0, aread, v_mains);

	adc_perf_data.sample_size_adc_0 =
		get_even_max_div(MAX_SEQ_SAMPLES, adc_perf_data.seq_idx_adc_0);
	adc_perf_data.sample_time_adc_0 = adc_dma_get_sampling_time(adc_perf_data.dev_adc_0);

	trigger_dev = device_get_binding(adc_dma_get_trig_dev(adc_perf_data.dev_adc_0));
	if (!trigger_dev) {
		LOG_ERR("no trigger device");
		return -ENXIO;
	}

	seq_cfg.callback = on_seq_done;
	seq_cfg.len = adc_perf_data.seq_idx_adc_0;
	seq_cfg.buffer = buf_adc_0;
	seq_cfg.buffer_size = sizeof(buf_adc_0);
	seq_cfg.samples = adc_perf_data.sample_size_adc_0;

	r = adc_dma_read(adc_perf_data.dev_adc_0, &seq_cfg);
	if (r < 0) {
		LOG_ERR("cannot start reading");
		return r;
	}

	adc_perf_data.sample_interval_adc_0 = adc_trigger_get_interval(trigger_dev);

	LOG_INF("channels          : %" PRIu8, adc_perf_data.seq_idx_adc_0);
	LOG_INF("sample time       : %" PRIu32 " ns", adc_perf_data.sample_time_adc_0);
	LOG_INF("sample interval   : %" PRIu32 " ns", adc_perf_data.sample_interval_adc_0);
	LOG_INF("Total samples     : %" PRIu32, adc_perf_data.sample_size_adc_0);

	return 0;
}

static void process_udp(void)
{
	while (1) {
		struct sockaddr addr;
		socklen_t addr_len = sizeof(addr);
		struct net_buf *buf;
		struct sockaddr *buf_ud;
		int received;

		received = recvfrom(adc_perf_data.udp_socket, adc_perf_data.recv_buf,
				    sizeof(adc_perf_data.recv_buf), 0, &addr, &addr_len);
		if (received < 0) {
			LOG_ERR("reception error %d", errno);
			continue;
		}

		buf = net_buf_alloc(&rx_buf_pool, K_NO_WAIT);
		if (!buf) {
			LOG_ERR("cannot allocate buffer");
			continue;
		}

		net_buf_add_mem(buf, adc_perf_data.recv_buf, received);
		buf_ud = net_buf_user_data(buf);
		net_ipaddr_copy(buf_ud, &addr);

		net_buf_put(&adc_perf_data.udp_rx_fifo, buf);
		k_work_submit(&adc_perf_data.udp_rx_work);
	}
}

static inline int start_udp_server(void)
{
	int r;
	struct sockaddr_in addr;

	adc_perf_data.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (adc_perf_data.udp_socket < 0) {
		LOG_ERR("failed to create UDP socket %d", errno);
		return -errno;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	r = bind(adc_perf_data.udp_socket, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) {
		LOG_ERR("failed to bind UDP socket %d", errno);
		return -errno;
	}

	k_thread_start(udp_thread);
	k_delayed_work_submit(&adc_perf_data.client_recycle_work, K_MSEC(1000));

	return 0;
}

int adc_pef_init(void)
{
	int r;

	memset(&adc_perf_data, 0, sizeof(adc_perf_data_t));
	k_fifo_init(&adc_perf_data.udp_tx_fifo);
	k_fifo_init(&adc_perf_data.udp_rx_fifo);
	k_work_init(&adc_perf_data.udp_tx_work, udp_tx_work);
	k_work_init(&adc_perf_data.udp_rx_work, udp_rx_work);
	k_delayed_work_init(&adc_perf_data.client_recycle_work, client_recycle_work);

	BCB_ADC_INIT(0);

	adc_dma_stop(adc_perf_data.dev_adc_0);
	adc_dma_set_reference(adc_perf_data.dev_adc_0, ADC_DMA_REF_EXTERNAL0);
	adc_dma_set_performance_level(adc_perf_data.dev_adc_0, ADC_DMA_PERF_LEVEL_3);

	r = start_adc();
	if (r < 0) {
		LOG_ERR("cannot read ADC: %d", r);
		return r;
	}

	start_udp_server();

	return 0;
}
