/**
 * @file uds_dynamic_did.c
 * @brief Implementierung von Service 0x2C (Dynamically Define Data Identifier)
 */

#include <zephyr/kernel.h>
#include <string.h>
#include "uds_types.h"
#include "uds_app_interface.h"

typedef struct {
	uint16_t source_did;
	uint8_t position;
	uint8_t length;
} dynamic_entry_t;

typedef struct {
	uint16_t dyn_did;
	dynamic_entry_t entries[4];
	uint8_t entry_count;
} dynamic_did_map_t;

static dynamic_did_map_t dyn_map;

void uds_handle_define_dynamic_did(uint8_t *req, size_t len, void (*send_cb)(const uint8_t *, size_t), void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	if (len < 9) { nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT); return; }

	uint8_t sub_function = req[1];
	if (sub_function != 0x01) { nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED); return; }

	dyn_map.dyn_did = ((uint16_t)req[2] << 8) | req[3];
	dyn_map.entries[0].source_did = ((uint16_t)req[4] << 8) | req[5];
	dyn_map.entries[0].position = req[6];
	dyn_map.entries[0].length = req[7];
	dyn_map.entry_count = 1;

	uint8_t pos_buf[] = {0x6C, sub_function, req[2], req[3]};
	send_cb(pos_buf, 4);
}

int uds_read_dynamic_did_payload(uint16_t did, uint8_t *buf_out, size_t *len_out)
{
	if (dyn_map.dyn_did != did || dyn_map.entry_count == 0) return -1;

	size_t total_len = 0;
	uint8_t temp_src_buf[64];
	size_t src_len;

	for (int i = 0; i < dyn_map.entry_count; i++) {
		int ret = uds_app_read_did(dyn_map.entries[i].source_did, temp_src_buf, &src_len, sizeof(temp_src_buf));
		if (ret == 0) {
			memcpy(&buf_out[total_len], &temp_src_buf[dyn_map.entries[i].position - 1], dyn_map.entries[i].length);
			total_len += dyn_map.entries[i].length;
		}
	}
	*len_out = total_len;
	return 0;
}
