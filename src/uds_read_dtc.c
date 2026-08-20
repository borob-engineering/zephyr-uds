/**
 * @file uds_read_dtc.c
 * @brief UDS Service 0x19 mit RAM-Speicher und Erweiterung um Freeze-Frames (0x04)
 */

#include "uds_read_dtc.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define MOCK_DTC_COUNT 2
static uds_dtc_t mock_dtc_server[MOCK_DTC_COUNT];

#define DTC_STATUS_AVAILABILITY_MASK 0x09 

void uds_read_dtc_init(void)
{
	mock_dtc_server[0].code[0] = 0x12;
	mock_dtc_server[0].code[1] = 0x34;
	mock_dtc_server[0].code[2] = 0x56;
	mock_dtc_server[0].status  = 0x09;

	mock_dtc_server[1].code[0] = 0xC1;
	mock_dtc_server[1].code[1] = 0x00;
	mock_dtc_server[1].code[2] = 0x00;
	mock_dtc_server[1].status  = 0x08;
}

void uds_read_dtc_clear_all(uint32_t dtc_group)
{
	for (int i = 0; i < MOCK_DTC_COUNT; i++) {
		if (dtc_group == 0xFFFFFF) {
			mock_dtc_server[i].status = 0x00;
		}
	}
}

void uds_read_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                         void (*send_cb)(const uint8_t *, size_t), 
                         void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	if (len < 2) { nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); return; }

	uint8_t sub_function = req[1];

	if (sub_function == 0x02) { /* reportDTCByStatusMask */
		if (len < 3) { nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); return; }
		uint8_t client_mask = req[2];
		size_t tx_idx = 0;

		tx_buf[tx_idx++] = sid + 0x40; tx_buf[tx_idx++] = sub_function; tx_buf[tx_idx++] = DTC_STATUS_AVAILABILITY_MASK;

		for (int i = 0; i < MOCK_DTC_COUNT; i++) {
			if (mock_dtc_server[i].status != 0x00 && (mock_dtc_server[i].status & client_mask) != 0) {
				tx_buf[tx_idx++] = mock_dtc_server[i].code[0];
				tx_buf[tx_idx++] = mock_dtc_server[i].code[1];
				tx_buf[tx_idx++] = mock_dtc_server[i].code[2];
				tx_buf[tx_idx++] = mock_dtc_server[i].status;
			}
		}
		send_cb(tx_buf, tx_idx);
		return;
	} 
	
	if (sub_function == 0x04) { /* ERWEITERUNG 1: reportDTCStoredDataByDTCNumber */
		if (len < 6) { nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); return; }
		uint32_t req_dtc = ((uint32_t)req[2] << 16) | ((uint32_t)req[3] << 8) | req[4];
		uint8_t record_num = req[5];
		size_t out_len = 0;

		tx_buf[0] = sid + 0x40; tx_buf[1] = sub_function;
		tx_buf[2] = req[2]; tx_buf[3] = req[3]; tx_buf[4] = req[4];

		int ret = uds_app_get_freeze_frame(req_dtc, record_num, &tx_buf[5], &out_len, UDS_BUFF_SIZE - 5);
		if (ret == 0) {
			send_cb(tx_buf, 5 + out_len);
		} else {
			nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
		}
		return;
	}

	nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
}
