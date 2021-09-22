#include "adc_perf.h"
#include <lib/bcb_macros.h>
#include <drivers/adc_dma.h>
#include <drivers/adc_trigger.h>
#include <device.h>
#include <devicetree.h>
#include <sys/util.h>
#include <fcntl.h>
#include <net/socket.h>
#include <net/net_ip.h>
#include <net/buf.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <tinycbor/cbor.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_perf);

#define BCB_ADC_DEV(inst) (adc_perf_data.dev_adc_##inst)
#define BCB_ADC_SEQ_IDX(inst) (adc_perf_data.seq_idx_adc_##inst)

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

#define SERVER_PORT 5555
#define CLIENT_QUEUE 2
#define MAX_CLIENTS 2
#define MAX_POLLFDS (MAX_CLIENTS + 1)
#define MAX_SEQ_SAMPLES 512
#define STACK_SIZE 1024
#define THREAD_PRIORITY K_PRIO_COOP(8)

typedef struct adc_perf_data {
	struct device *dev_adc_0;
	uint8_t seq_idx_adc_0;
	uint32_t sample_size_adc_0;
	uint32_t sample_time_adc_0;
	uint32_t sample_interval_adc_0;

	uint32_t sample_block_seq;
	struct k_work client_io_work;
	struct k_fifo tx_fifo;
	int server_socket;
	struct pollfd pollfds[MAX_CLIENTS + 1];
	uint32_t last_ping_time[MAX_CLIENTS + 1];
	uint8_t recv_buf[128];
	uint8_t cbor_buf[2048];
} adc_perf_data_t;

static adc_perf_data_t adc_perf_data;

static uint16_t buf_adc_0[MAX_SEQ_SAMPLES] __attribute__((aligned(2)));

NET_BUF_POOL_DEFINE(tx_buf_pool, 4, MAX_SEQ_SAMPLES * sizeof(uint16_t), sizeof(uint64_t), NULL);
K_MSGQ_DEFINE(client_notify_msgq, sizeof(int), 4, 4);

static void process_tcp(void);

K_THREAD_DEFINE(tcp_thread, STACK_SIZE, process_tcp, NULL, NULL, NULL, THREAD_PRIORITY, 0, -1);

static void on_seq_done(struct device *dev, volatile void *buffer, uint32_t samples)
{
	struct net_buf *buf;

	adc_perf_data.sample_block_seq++;

	buf = net_buf_alloc(&tx_buf_pool, K_NO_WAIT);
	if (!buf) {
		/* If there is no space in the pool, we remove the oldest sample. */
		buf = net_buf_get(&adc_perf_data.tx_fifo, K_NO_WAIT);
		if (!buf) {
			return;
		}
	}

	net_buf_reset(buf);
	net_buf_add_mem(buf, (void *)buffer, samples * sizeof(uint16_t));
	/* user data buffer is aligned to void * */
	*((uint32_t *)net_buf_user_data(buf)) = adc_perf_data.sample_block_seq;

	net_buf_put(&adc_perf_data.tx_fifo, buf);
	k_work_submit(&adc_perf_data.client_io_work);
}

static void pollfds_reset()
{
	int i;
	for (i = 0; i < MAX_POLLFDS; i++) {
		adc_perf_data.pollfds[i].fd = -1;
		adc_perf_data.last_ping_time[i] = 0;
	}
}

static int pollfds_add(int fd)
{
	int i;
	for (i = 0; i < MAX_POLLFDS; i++) {
		if (adc_perf_data.pollfds[i].fd == -1) {
			adc_perf_data.pollfds[i].fd = fd;
			adc_perf_data.pollfds[i].events = POLLIN | POLLOUT;
			adc_perf_data.last_ping_time[i] = k_uptime_get_32();
			return 0;
		}
	}

	return -ENOMEM;
}

static int pollfds_del(int fd)
{
	int i;
	for (i = 0; i < MAX_POLLFDS; i++) {
		if (adc_perf_data.pollfds[i].fd == fd) {
			adc_perf_data.pollfds[i].fd = -1;
			adc_perf_data.last_ping_time[i] = 0;
			return 0;
		}
	}

	return -ENOENT;
}

static int set_blocking(int fd, bool val)
{
	int fl, res;

	fl = fcntl(fd, F_GETFL, 0);
	if (fl == -1) {
		LOG_ERR("fcntl(F_GETFL): %d", errno);
		return -EIO;
	}

	if (val) {
		fl &= ~O_NONBLOCK;
	} else {
		fl |= O_NONBLOCK;
	}

	res = fcntl(fd, F_SETFL, fl);
	if (fl == -1) {
		LOG_ERR("fcntl(F_SETFL): %d", errno);
		return -EIO;
	}

	return 0;
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
	cbor_encode_byte_string(&array_encoder, (uint8_t *)buf->data, buf->len);

	cbor_encoder_close_container(&encoder, &array_encoder);

	return writer.bytes_written;
}

static int client_io(struct net_buf *buf)
{
	int i, r;
	uint32_t bytes_written;

	if (buf) {
		bytes_written = create_cbor_frame(buf);
	} else {
		bytes_written = 0;
	}

	r = poll(adc_perf_data.pollfds, MAX_POLLFDS, 0);
	if (r < 0) {
		LOG_ERR("poll error: %d", errno);
		return r;
	}

	for (i = 0; i < MAX_POLLFDS; i++) {
		if (adc_perf_data.pollfds[i].fd == -1) {
			continue;
		}

		if ((adc_perf_data.pollfds[i].revents & POLLERR) ||
		    (adc_perf_data.pollfds[i].revents & POLLHUP)) {
			close(adc_perf_data.pollfds[i].fd);
			pollfds_del(adc_perf_data.pollfds[i].fd);
			continue;
		}

		if ((adc_perf_data.pollfds[i].revents & POLLOUT) && buf) {
			r = send(adc_perf_data.pollfds[i].fd, adc_perf_data.cbor_buf, bytes_written,
				 0);
			if (r < 0) {
				LOG_ERR("send error %d", errno);
				close(adc_perf_data.pollfds[i].fd);
				pollfds_del(adc_perf_data.pollfds[i].fd);
				continue;
			}
		}

		if (adc_perf_data.pollfds[i].revents & POLLIN) {
			r = recv(adc_perf_data.pollfds[i].fd, adc_perf_data.recv_buf,
				 sizeof(adc_perf_data.recv_buf), 0);

			if (r < 0) {
				LOG_ERR("recive error: %d [%d]", errno,
					adc_perf_data.pollfds[i].fd);
				continue;
			}

			if (r == 0) {
				LOG_INF("connection closed [%d]", adc_perf_data.pollfds[i].fd);
				close(adc_perf_data.pollfds[i].fd);
				pollfds_del(adc_perf_data.pollfds[i].fd);
				continue;
			}

			adc_perf_data.last_ping_time[i] = k_uptime_get_32();
		}
	}

	return 0;
}

static void client_io_work(struct k_work *work)
{
	struct net_buf *buf;
	int client;
	uint32_t blocks_sent = 0;

	while (!k_msgq_get(&client_notify_msgq, &client, K_NO_WAIT)) {
		/* New client */
		int r;

		r = set_blocking(client, false);
		if (r < 0) {
			LOG_ERR("cannot set blocking error %d", r);
			close(client);
			continue;
		}

		r = pollfds_add(client);
		if (r < 0) {
			LOG_ERR("too many connections : %d", r);
			close(client);
			continue;
		}
	}

	while ((buf = net_buf_get(&adc_perf_data.tx_fifo, K_NO_WAIT)) != NULL) {
		client_io(buf);
		net_buf_unref(buf);
		blocks_sent++;
	}

	if (!blocks_sent) {
		/* 
		 * Nothing is sent to clients. 
		 *  Run IO function to check incoming data from clients 
		 */
		client_io(NULL);
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
	BCB_ADC_SEQ_ADD(0, aread, i_low_gain);
	BCB_ADC_SEQ_ADD(0, aread, i_high_gain);
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

	LOG_INF("sample time       : %" PRIu32 " ns", adc_perf_data.sample_time_adc_0);
	LOG_INF("trigger interval  : %" PRIu32 " ns", adc_perf_data.sample_interval_adc_0);
	LOG_INF("samples size (ADC): %" PRIu32, adc_perf_data.sample_size_adc_0);

	return 0;
}

static void process_tcp(void)
{
	int r;
	struct sockaddr_in addr;

	adc_perf_data.server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (adc_perf_data.server_socket < 0) {
		LOG_ERR("failed to create socket: %d", errno);
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	r = bind(adc_perf_data.server_socket, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) {
		LOG_ERR("failed to bind: %d", errno);
		return;
	}

	r = listen(adc_perf_data.server_socket, CLIENT_QUEUE);
	if (r < 0) {
		LOG_ERR("failed to listen: %d", errno);
		return;
	}

	LOG_INF("server started");

	do {
		int client;
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		void *addr;
		char ip_addr_str[32];

		client = accept(adc_perf_data.server_socket,
				(struct sockaddr *)&client_addr, &client_addr_len);
		if (client < 0) {
			LOG_ERR("accept error: %d", errno);
			continue;
		}
	
		addr = &((struct sockaddr_in *)&client_addr)->sin_addr;
		inet_ntop(client_addr.ss_family, addr, ip_addr_str,
				sizeof(ip_addr_str));

		LOG_INF("connected from %s [%d]", log_strdup(ip_addr_str), client);
		/* send to client to management work */
		r = k_msgq_put(&client_notify_msgq, &client, K_NO_WAIT);
		if (r < 0) {
			LOG_ERR("client msgq error: %d", errno);
			continue;
		}
		k_work_submit(&adc_perf_data.client_io_work);


	} while (1);
}

int adc_pef_init(void)
{
	int r;

	memset(&adc_perf_data, 0, sizeof(adc_perf_data_t));
	k_fifo_init(&adc_perf_data.tx_fifo);
	k_work_init(&adc_perf_data.client_io_work, client_io_work);

	BCB_ADC_INIT(0);

	adc_dma_stop(adc_perf_data.dev_adc_0);
	adc_dma_set_reference(adc_perf_data.dev_adc_0, ADC_DMA_REF_EXTERNAL0);
	adc_dma_set_performance_level(adc_perf_data.dev_adc_0, ADC_DMA_PERF_LEVEL_4);

	pollfds_reset();

	r = start_adc();
	if (r < 0) {
		LOG_ERR("cannot read ADC: %d", r);
		return r;
	}

	k_thread_start(tcp_thread);

	return 0;
}

static int cmd_start_adc_handler(const struct shell *shell, size_t argc, char **argv)
{
	start_adc();

	return 0;
}

static int cmd_stop_adc_handler(const struct shell *shell, size_t argc, char **argv)
{
	adc_dma_stop(adc_perf_data.dev_adc_0);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(adc_sub, SHELL_CMD(start, NULL, "Start ADC", cmd_start_adc_handler),
			       SHELL_CMD(stop, NULL, "Stop ADC", cmd_stop_adc_handler),
			       SHELL_SUBCMD_SET_END /* Array terminated. */);

SHELL_CMD_REGISTER(adc, &adc_sub, "ADC commands", NULL);

/**
 * Use two system work queues:
 * 1. SendWork - Sending data to clients
 * 2. ManagementWork - Handle new clients, remove timeedout clients, etc.
 *
 * Accept new clients in the adc_perf_server_loop or in a diffent thread.
 * Use message queue for this purpose.
 * Then submit a work item for the ManagementWork.
 *
 * In the SendWork,
 * poll() all the sockets and send data only to the ones which can accept new data.   
 * 
 */