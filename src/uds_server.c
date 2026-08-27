/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UDS Core Server & ISO-TP Connection Handling for Zephyr RTOS v4.4.0.
 *
 * This file handles the central network interface loop, binds to physical
 * and functional ISO-TP addresses, and routes diagnostic requests to their
 * respective sub-service handlers.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/canbus/isotp.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "uds_types.h"
#include "uds_session.h"
#include "uds_security.h"
#include "uds_routine.h"
#include "uds_reset.h"
#include "uds_clear_dtc.h"
#include "uds_read_dtc.h"
#include "uds_write_did.h"
#include "uds_flash_pipeline.h"
#include "uds_dynamic_did.h"
#include "uds_periodic.h"
#include "uds_iocontrol.h"
#include "uds_app_interface.h"

LOG_MODULE_REGISTER(uds_server, LOG_LEVEL_INF);

#define UDS_RX_THREAD_STACK_SIZE 2048
#define UDS_RX_THREAD_PRIORITY   7

static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct isotp_recv_ctx rx_ctx;
static struct isotp_recv_ctx func_rx_ctx;
static struct isotp_send_ctx tx_ctx;

static const struct isotp_fc_opts fc_opts = {
	.bs = 8,
	.stmin = 10
};

static const struct isotp_msg_id rx_link = {
	.std_id = CONFIG_UDS_CAN_RX_ID,
	.flags = 0,
	.ext_addr = 0,
	.dl = 8
};

static const struct isotp_msg_id tx_link = {
	.std_id = CONFIG_UDS_CAN_TX_ID,
	.flags = 0,
	.ext_addr = 0,
	.dl = 8
};

static const struct isotp_msg_id func_rx_link = {
	.std_id = CONFIG_UDS_CAN_FUNC_RX_ID,
	.flags = 0,
	.ext_addr = 0,
	.dl = 8
};

static uint8_t uds_rx_buf[UDS_BUFF_SIZE];
static uint8_t uds_tx_buf[UDS_BUFF_SIZE];

K_THREAD_STACK_DEFINE(uds_rx_stack, UDS_RX_THREAD_STACK_SIZE);
static struct k_thread uds_rx_thread_data;

/** @brief Global tracking of the currently processed request addressing type. */
static uds_addressing_t current_msg_addressing = UDS_ADDR_PHYSICAL;

/**
 * @brief Sends a positive response payload over the physical ISO-TP link.
 *
 * @param data Pointer to the transmission buffer payload.
 * @param len  Length of the payload in bytes.
 */
static void uds_send_response(const uint8_t *data, size_t len)
{
	int ret;

	ret = isotp_send(&tx_ctx, can_dev, data, len, &tx_link, &rx_link, NULL, NULL);
	if (ret != ISOTP_N_OK) {
		LOG_ERR("ISO-TP Send transmission error: %d", ret);
	}
}

/**
 * @brief Generates and transmits a standard UDS Negative Response Code (NRC).
 *
 * Implements functional addressing suppression rules according to ISO 14229-1.
 *
 * @param sid The Service Identifier that caused the error.
 * @param nrc The precise Negative Response Code to send.
 */
static void uds_send_nrc(uint8_t sid, uint8_t nrc)
{
	uint8_t nrc_buf[] = {0x7F, sid, nrc};

	if (current_msg_addressing == UDS_ADDR_FUNCTIONAL) {
		if (nrc != UDS_NRC_RESPONSE_PENDING) {
			LOG_DBG("Functional NRC 0x%02X suppressed per ISO standard.", nrc);
			return;
		}
	}

	uds_send_response(nrc_buf, sizeof(nrc_buf));
}

/**
 * @brief Triggers an unprompted ResponseOnEvent notification (Service 0x86).
 *
 * @param event_type The configured event window code.
 * @param payload    Pointer to the event specific diagnostic data.
 * @param len        Length of the event data.
 */
void uds_trigger_event_response(uint8_t event_type, const uint8_t *payload, size_t len)
{
	uint8_t event_buf[UDS_BUFF_SIZE];

	event_buf[0] = 0x86;
	event_buf[1] = event_type;

	if (len > 0 && payload != NULL && (len + 2) < UDS_BUFF_SIZE) {
		memcpy(&event_buf[2], payload, len);
	}

	uds_send_response(event_buf, 2 + len);
}
/**
 * @brief Routes and processes the incoming decoupled UDS diagnostic frame.
 *
 * @param req       Pointer to raw received request byte stream.
 * @param len       Length of received byte stream.
 * @param addr_type Addressing method used by the client.
 */
static void uds_process_request(uint8_t *req, size_t len, uds_addressing_t addr_type)
{
	uint8_t sid;
	uint8_t req_sess;
	uint16_t read_did;
	size_t d_len = 0;
	int r_ret;

	if (len == 0 || req == NULL) {
		return;
	}

	current_msg_addressing = addr_type;
	uds_session_refresh_timer();

	sid = req[0];
	LOG_INF("UDS Request [%s]. SID: 0x%02X",
		(addr_type == UDS_ADDR_PHYSICAL) ? "PHYS" : "FUNC", sid);

	switch (sid) {
	case 0x10:
		if (len < 2) {
			uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			break;
		}
		req_sess = req[1];
		if (req_sess != 0x01 && req_sess != 0x02 && req_sess != 0x03) {
			uds_send_nrc(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
			break;
		}
		uds_security_reset_lock();
		uds_session_set((uds_session_type_t)req_sess);
		uds_tx_buf[0] = sid + 0x40;
		uds_tx_buf[1] = req_sess;
		uds_send_response(uds_tx_buf, 2);
		break;

	case 0x11:
		uds_reset_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x14:
		uds_clear_dtc_handle(req, len, uds_send_response, uds_send_nrc);
		break;

	case 0x19:
		uds_read_dtc_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x22:
		if (len < 3) {
			uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			break;
		}
		read_did = ((uint16_t)req[1] << 8) | req[2];
		
		/* KORREKTUR: Nutze uds_send_response statt dem fehlerhaften send_cb */
		if (uds_read_dynamic_did_payload(read_did, &uds_tx_buf[3], &d_len) == 0) {
			uds_tx_buf[0] = sid + 0x40;
			uds_tx_buf[1] = req[1];
			uds_tx_buf[2] = req[2];
			uds_send_response(uds_tx_buf, 3 + d_len);
			break;
		}
		
		r_ret = uds_app_read_did(read_did, &uds_tx_buf[3], &d_len, UDS_BUFF_SIZE - 3);
		if (r_ret == 0) {
			uds_tx_buf[0] = sid + 0x40;
			uds_tx_buf[1] = req[1];
			uds_tx_buf[2] = req[2];
			uds_send_response(uds_tx_buf, 3 + d_len);
		} else if (r_ret == -ENOMEM) {
			uds_send_nrc(sid, UDS_NRC_RESPONSE_PENDING);
		} else {
			uds_send_nrc(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
		}
		break;

	case 0x27:
		if (addr_type == UDS_ADDR_FUNCTIONAL) {
			uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			break;
		}
		uds_security_handle_request(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x2E:
		uds_write_did_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x2A:
		uds_handle_periodic_request(req, len, uds_send_response, uds_send_nrc);
		break;

	case 0x2C:
		uds_handle_define_dynamic_did(req, len, uds_send_response, uds_send_nrc);
		break;
	
	case 0x2F:
		if (addr_type == UDS_ADDR_FUNCTIONAL) {
			uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			break;
		}
		uds_handle_io_control(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x31:
		uds_routine_handle_control(req, len, uds_send_response, uds_send_nrc);
		break;

	case 0x34:
		if (addr_type == UDS_ADDR_FUNCTIONAL) {
			uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			break;
		}
		uds_handle_request_download(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x36:
		if (addr_type == UDS_ADDR_FUNCTIONAL) {
			uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			break;
		}
		uds_handle_transfer_data(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x37:
		if (addr_type == UDS_ADDR_FUNCTIONAL) {
			uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			break;
		}
		uds_handle_request_transfer_exit(req, len, uds_tx_buf, uds_send_response,
						 uds_send_nrc);
		break;

	case 0x86:
		if (len < 3) {
			uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			break;
		}
		uds_tx_buf[0] = sid + 0x40;
		uds_tx_buf[1] = req[1];
		uds_tx_buf[2] = req[2];
		uds_send_response(uds_tx_buf, 3);
		break;

	case 0x3E:
		if (len < 2) {
			uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			break;
		}
		if ((req[1] & 0x80) == 0) {
			uds_tx_buf[0] = sid + 0x40;
			uds_tx_buf[1] = req[1] & 0x7F;
			uds_send_response(uds_tx_buf, 2);
		}
		break;

	default:
		uds_send_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
		break;
	}
}

/**
 * @brief Native Zephyr worker thread processing incoming ISO-TP messages.
 *
 * Concurrently samples physical and functional context messages via non-blocking polls.
 */
static void uds_rx_thread(void *p1, void *p2, void *p3)
{
	int received_bytes;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (isotp_bind(&rx_ctx, can_dev, &rx_link, &tx_link, &fc_opts, K_FOREVER) != ISOTP_N_OK) {
		return;
	}
	if (isotp_bind(&func_rx_ctx, can_dev, &func_rx_link, &tx_link, &fc_opts, K_FOREVER) !=
	    ISOTP_N_OK) {
		return;
	}

	for (;;) {
		received_bytes = isotp_recv(&rx_ctx, uds_rx_buf, sizeof(uds_rx_buf), K_MSEC(10));
		if (received_bytes >= 0) {
			uds_process_request(uds_rx_buf, received_bytes, UDS_ADDR_PHYSICAL);
		}

		received_bytes = isotp_recv(&func_rx_ctx, uds_rx_buf, sizeof(uds_rx_buf),
					    K_MSEC(10));
		if (received_bytes >= 0) {
			uds_process_request(uds_rx_buf, received_bytes, UDS_ADDR_FUNCTIONAL);
		}
	}
}

/**
 * @brief Public initialization function for the global UDS module.
 *
 * Verifies CAN hardware readiness, initializes sub-modules, and spawns the RX thread.
 *
 * @return 0 on success, negative errno code on hardware/subsystem initialization failure.
 */
int uds_init(void)
{
	int ret;

	if (!device_is_ready(can_dev)) {
		return -ENODEV;
	}

	ret = can_start(can_dev);
	if (ret != 0) {
		LOG_ERR("Failed to start CAN controller: %d", ret);
	}

	uds_session_init();
	uds_security_init();
	uds_routine_init();
	uds_reset_init();
	uds_clear_dtc_init();
	uds_read_dtc_init();
	uds_flash_pipeline_init();

	k_thread_create(&uds_rx_thread_data, uds_rx_stack, K_THREAD_STACK_SIZEOF(uds_rx_stack),
			uds_rx_thread, NULL, NULL, NULL, UDS_RX_THREAD_PRIORITY, 0, K_NO_WAIT);
					
	return 0;
}
