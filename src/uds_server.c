/**
 * @file uds_server.c
 * @brief UDS Core Server & ISO-TP Connection Handling für Zephyr RTOS v4.4.0
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
#include "uds_iocontrol.h"
#include "uds_app_interface.h"

LOG_MODULE_REGISTER(uds_server, LOG_LEVEL_INF);

#define UDS_RX_THREAD_STACK_SIZE 2048
#define UDS_RX_THREAD_PRIORITY   7

static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct isotp_recv_ctx rx_ctx;
static struct isotp_recv_ctx func_rx_ctx;
static struct isotp_send_ctx tx_ctx;

static const struct isotp_fc_opts fc_opts = {.bs = 8, .stmin = 10};

static const struct isotp_msg_id rx_link = {.std_id = CONFIG_UDS_CAN_RX_ID, .flags = 0, .ext_addr = 0, .dl = 8};
static const struct isotp_msg_id tx_link = {.std_id = CONFIG_UDS_CAN_TX_ID, .flags = 0, .ext_addr = 0, .dl = 8};
static const struct isotp_msg_id func_rx_link = {.std_id = CONFIG_UDS_CAN_FUNC_RX_ID, .flags = 0, .ext_addr = 0, .dl = 8};

static uint8_t uds_rx_buf[UDS_BUFF_SIZE];
static uint8_t uds_tx_buf[UDS_BUFF_SIZE];

K_THREAD_STACK_DEFINE(uds_rx_stack, UDS_RX_THREAD_STACK_SIZE);
static struct k_thread uds_rx_thread_data;

static uds_addressing_t current_msg_addressing = UDS_ADDR_PHYSICAL;

static void uds_send_response(const uint8_t *data, size_t len)
{
	int ret = isotp_send(&tx_ctx, can_dev, data, len, &tx_link, &rx_link, NULL, NULL);
	if (ret != ISOTP_N_OK) LOG_ERR("ISO-TP Sendefehler: %d", ret);
}

/**
 * @brief Erzeugt und sendet einen Negative Response Code (NRC) mit striktem Serien-Funktional-Filter
 */
static void uds_send_nrc(uint8_t sid, uint8_t nrc)
{
	/* STRIKTE ISO 14229-1 SERIENREGEL: Wenn die Anfrage FUNKTIONAL (0x7DF) hereinkam, 
	 * darf ein NRC NUR DANN gesendet werden, wenn es ein RESPONSE_PENDING (0x78) ist. 
	 * Alle anderen Fehler (Länge, Security, etc.) werden komplett verschwiegen. */
	if (current_msg_addressing == UDS_ADDR_FUNCTIONAL) {
		if (nrc != UDS_NRC_RESPONSE_PENDING) {
			LOG_DBG("Funktionale Adressierung: NRC 0x%02X fuer SID 0x%02X regelkonform unterdrueckt.", nrc, sid);
			return;
		}
	}

	uint8_t nrc_buf[] = {0x7F, sid, nrc};
	uds_send_response(nrc_buf, sizeof(nrc_buf));
}

static void uds_process_request(uint8_t *req, size_t len, uds_addressing_t addr_type)
{
	if (len == 0 || req == NULL) return;
	current_msg_addressing = addr_type;
	uds_session_refresh_timer();

	uint8_t sid = req[0];

	switch (sid) {
	case 0x10:
		if (len < 2) { uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); break; }
		uint8_t req_sess = req[1];
		if (req_sess != 0x01 && req_sess != 0x02 && req_sess != 0x03) { uds_send_nrc(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED); break; }
		uds_security_reset_lock();
		uds_session_set((uds_session_type_t)req_sess);
		uds_tx_buf[0] = sid + 0x40; uds_tx_buf[1] = req_sess;
		uds_send_response(uds_tx_buf, 2);
		break;

	case 0x11: uds_reset_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc); break;
	case 0x14: uds_clear_dtc_handle(req, len, uds_send_response, uds_send_nrc); break;
	case 0x19: uds_read_dtc_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc); break;

	case 0x22: /* Read DID */
		if (len < 3) { uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); break; }
		uint16_t did = ((uint16_t)req[1] << 8) | req[2];
		size_t out_len = 0;
		
		int ret = uds_app_read_did(did, &uds_tx_buf[3], &out_len, UDS_BUFF_SIZE - 3);
		if (ret == 0) {
			uds_tx_buf[0] = sid + 0x40; uds_tx_buf[1] = req[1]; uds_tx_buf[2] = req[2];
			uds_send_response(uds_tx_buf, 3 + out_len);
		} else if (ret == -ENOMEM) {
			uds_send_nrc(sid, UDS_NRC_RESPONSE_PENDING);
		} else {
			uds_send_nrc(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
		}
		break;

	case 0x27:
		if (addr_type == UDS_ADDR_FUNCTIONAL) { uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT); break; }
		uds_security_handle_request(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x2E: uds_write_did_handle(req, len, uds_tx_buf, uds_send_response, uds_send_nrc); break;
	case 0x2F:
		if (addr_type == UDS_ADDR_FUNCTIONAL) { uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT); break; }
		uds_handle_io_control(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x31: uds_routine_handle_control(req, len, uds_send_response, uds_send_nrc); break;
	case 0x34:
		if (addr_type == UDS_ADDR_FUNCTIONAL) { uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT); break; }
		uds_handle_request_download(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x36:
		if (addr_type == UDS_ADDR_FUNCTIONAL) { uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT); break; }
		uds_handle_transfer_data(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x37:
		if (addr_type == UDS_ADDR_FUNCTIONAL) { uds_send_nrc(sid, UDS_NRC_CONDITIONS_NOT_CORRECT); break; }
		uds_handle_request_transfer_exit(req, len, uds_tx_buf, uds_send_response, uds_send_nrc);
		break;

	case 0x3E:
		if (len < 2) { uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); break; }
		if ((req[1] & 0x80) == 0) { uds_tx_buf[0] = sid + 0x40; uds_tx_buf[1] = req[1] & 0x7F; uds_send_response(uds_tx_buf, 2); }
		break;

	default: uds_send_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED); break;
	}
}

static void uds_rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
	int received_bytes;

	if (isotp_bind(&rx_ctx, can_dev, &rx_link, &tx_link, &fc_opts, K_FOREVER) != ISOTP_N_OK) return;
	if (isotp_bind(&func_rx_ctx, can_dev, &func_rx_link, &tx_link, &fc_opts, K_FOREVER) != ISOTP_N_OK) return;

	while (1) {
		received_bytes = isotp_recv(&rx_ctx, uds_rx_buf, sizeof(uds_rx_buf), K_MSEC(10));
		if (received_bytes >= 0) uds_process_request(uds_rx_buf, received_bytes, UDS_ADDR_PHYSICAL);

		received_bytes = isotp_recv(&func_rx_ctx, uds_rx_buf, sizeof(uds_rx_buf), K_MSEC(10));
		if (received_bytes >= 0) uds_process_request(uds_rx_buf, received_bytes, UDS_ADDR_FUNCTIONAL);
	}
}

int uds_init(void)
{
	if (!device_is_ready(can_dev)) return -ENODEV;

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
