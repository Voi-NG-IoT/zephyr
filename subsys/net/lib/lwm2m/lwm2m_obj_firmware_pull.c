/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_obj_firmware_pull
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <string.h>

#include "lwm2m_pull_context.h"
#include "lwm2m_engine.h"

static void set_update_result_from_error(int error_code)
{
	if (!error_code) {
		lwm2m_firmware_set_update_state(STATE_DOWNLOADED);
		return;
	}

	if (error_code == -ENOMEM) {
		lwm2m_firmware_set_update_result(RESULT_OUT_OF_MEM);
	} else if (error_code == -ENOSPC) {
		lwm2m_firmware_set_update_result(RESULT_NO_STORAGE);
	} else if (error_code == -EFAULT) {
		lwm2m_firmware_set_update_result(RESULT_INTEGRITY_FAILED);
	} else if (error_code == -ENOMSG) {
		lwm2m_firmware_set_update_result(RESULT_CONNECTION_LOST);
	} else if (error_code == -ENOTSUP) {
		lwm2m_firmware_set_update_result(RESULT_INVALID_URI);
	} else if (error_code == -EPROTONOSUPPORT) {
		lwm2m_firmware_set_update_result(RESULT_UNSUP_PROTO);
	} else {
		lwm2m_firmware_set_update_result(RESULT_UPDATE_FAILED);
	}
}

static struct firmware_pull_context fota_context = {
	.firmware_ctx = {
		.sock_fd = -1
	},
	.result_cb = set_update_result_from_error
};

static void socket_fault_cb(int error)
{
	int ret;

	LOG_ERR("FW update socket error: %d", error);

	lwm2m_engine_context_close(&firmware_ctx);

	/* Reopen the socket and retransmit the last request. */
	lwm2m_engine_context_init(&firmware_ctx);
	ret = lwm2m_socket_start(&firmware_ctx);
	if (ret < 0) {
		LOG_ERR("Failed to start a firmware-pull connection: %d", ret);
		goto error;
	}

	ret = transfer_request(&firmware_block_ctx,
			       NULL, LWM2M_MSG_TOKEN_GENERATE_NEW,
			       do_firmware_transfer_reply_cb);
	if (ret < 0) {
		LOG_ERR("Failed to send a retry packet: %d", ret);
		goto error;
	}

	return;

error:
	/* Abort retries. */
	firmware_retry = PACKET_TRANSFER_RETRY_MAX;
	set_update_result_from_error(ret);
	lwm2m_engine_context_close(&firmware_ctx);
}

/* TODO: */
int lwm2m_firmware_cancel_transfer(void)
{
	return 0;
}

int lwm2m_firmware_start_transfer(char *package_uri)
{
	fota_context.write_cb = lwm2m_firmware_get_write_cb();

	/* close old socket */
	if (fota_context.firmware_ctx.sock_fd > -1) {
		lwm2m_engine_context_close(&fota_context.firmware_ctx);
	}

	(void)memset(&fota_context.firmware_ctx, 0, sizeof(struct lwm2m_ctx));
	fota_context.firmware_ctx.sock_fd = -1;
	fota_context.firmware_ctx.fault_cb = socket_fault_cb;
	fota_context.retry = 0;
	k_work_init(&fota_context.firmware_work, firmware_transfer);
	lwm2m_firmware_set_update_state(STATE_DOWNLOADING);

	/* start file transfer work */
	strncpy(fota_context.uri, package_uri, LWM2M_PACKAGE_URI_LEN - 1);
	lwm2m_pull_context_start_transfer(&fota_context);
	lwm2m_firmware_set_update_state(STATE_DOWNLOADING);

	return 0;
}

/**
 * @brief Get the block context of the current firmware block.
 *
 * @return A pointer to the firmware block context
 */
struct coap_block_context *lwm2m_firmware_get_block_context()
{
	return &fota_context.block_ctx;
}
