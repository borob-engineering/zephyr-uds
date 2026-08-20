/**
 * @file uds_periodic.c
 * @brief Implementierung von Service 0x2A (Read Data By Periodic Identifier) via periodischem Kernel-Thread
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "uds_app_interface.h"

LOG_MODULE_REGISTER(uds_periodic, LOG_LEVEL_INF);

static uint8_t active_periodic_dids[5];
static uint8_t active_did_count = 0;
static void (*periodic_send_cb)(const uint8_t *, size_t) = NULL;

static void uds_periodic_thread_handler(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
	uint8_t periodic_tx_buf[16];
	size_t data_len;

	while (1) {
		k_msleep(10); /* Zyklischer Sendetakt: 10 ms */

		if (active_did_count == 0 || periodic_send_cb == NULL) continue;

		for (int i = 0; i < active_did_count; i++) {
			periodic_tx_buf[0] = 0x2A;
			periodic_tx_buf[1] = active_periodic_dids[i];
			
			int ret = uds_app_get_periodic_did(active_periodic_dids[i], &periodic_tx_buf[2], &data_len);
			if (ret == 0) {
				periodic_send_cb(periodic_tx_buf, 2 + data_len);
			}
		}
	}
}

K_THREAD_DEFINE(uds_periodic_id, 1024, uds_periodic_thread_handler, NULL, NULL, NULL, 6, 0, 0);

void uds_handle_periodic_request(uint8_t *req, size_t len, void (*send_cb)(const uint8_t *, size_t), void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	periodic_send_cb = send_cb;

	if (len < 3) { nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); return; }
	uint8_t transmission_mode = req[1];

	if (transmission_mode == 0x04) { /* stopTransmission */
		active_did_count = 0;
		uint8_t pos_buf[] = {0x6A, transmission_mode};
		send_cb(pos_buf, 2);
		return;
	}

	if (transmission_mode == 0x01) { /* sendAtFastRate */
		if (active_did_count >= 5) { nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE); return; }
		active_periodic_dids[active_did_count++] = req[2];
		
		uint8_t pos_buf[] = {0x6A, transmission_mode, req[2]};
		send_cb(pos_buf, 3);
	}
}
