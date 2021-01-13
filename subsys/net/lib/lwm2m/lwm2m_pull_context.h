#include <stdio.h>
#include <net/lwm2m.h>
#include <sys_clock.h>

#define LWM2M_PACKAGE_URI_LEN 255
struct firmware_pull_context;
struct firmware_pull_context {
	char uri[LWM2M_PACKAGE_URI_LEN];
	int retry;
	struct k_delayed_work firmware_work;
	struct lwm2m_ctx firmware_ctx;
	struct coap_block_context block_ctx;

	void (*result_cb)(struct firmware_pull_context *context,
                          int error_code);
	lwm2m_engine_set_data_cb_t write_cb;
};

int lwm2m_pull_context_start_transfer(struct firmware_pull_context *ctx,
                                      k_timeout_t timeout);
