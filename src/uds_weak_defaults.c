/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Safe default weak fallbacks for the generic UDS stack application interface.
 */

#include "uds_app_interface.h"
#include <zephyr/toolchain.h>
#include <errno.h>

__weak int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len)
{
	ARG_UNUSED(did);
	ARG_UNUSED(data_out);
	ARG_UNUSED(len_out);
	ARG_UNUSED(max_len);
	return -ENOENT; 
}

__weak int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len)
{
	ARG_UNUSED(did);
	ARG_UNUSED(data_in);
	ARG_UNUSED(len);
	return -EACCES;
}

__weak uint32_t uds_app_calculate_key(const uint8_t *seed, size_t len)
{
	ARG_UNUSED(seed);
	ARG_UNUSED(len);
	return 0x00000000;
}

__weak int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state,
			      size_t state_len, uint8_t *status_out)
{
	ARG_UNUSED(did);
	ARG_UNUSED(control_param);
	ARG_UNUSED(control_state);
	ARG_UNUSED(state_len);
	ARG_UNUSED(status_out);
	return -ENOENT;
}

__weak int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out)
{
	ARG_UNUSED(routine_id);
	ARG_UNUSED(info_out);
	return -ENOENT;
}

__weak int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out,
					    uint8_t *exit_info_out)
{
	ARG_UNUSED(routine_id);
	ARG_UNUSED(status_out);
	ARG_UNUSED(exit_info_out);
	return -ENOENT;
}

__weak int uds_app_flash_erase_target(uint32_t address, size_t size)
{
	ARG_UNUSED(address);
	ARG_UNUSED(size);
	return -EOPNOTSUPP;
}

__weak int uds_app_flash_write_block(uint32_t address_offset, const uint8_t *data, size_t len)
{
	ARG_UNUSED(address_offset);
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	return -EOPNOTSUPP;
}

__weak int uds_app_get_freeze_frame(uint32_t dtc, uint8_t record_num, uint8_t *data_out,
				     size_t *len_out, size_t max_len)
{
	ARG_UNUSED(dtc);
	ARG_UNUSED(record_num);
	ARG_UNUSED(data_out);
	ARG_UNUSED(len_out);
	ARG_UNUSED(max_len);
	return -ENOENT;
}

__weak int uds_app_verify_key_krypto(uint8_t security_level, const uint8_t *seed, size_t seed_len,
				     const uint8_t *received_key, size_t key_len)
{
	ARG_UNUSED(security_level);
	ARG_UNUSED(seed);
	ARG_UNUSED(seed_len);
	ARG_UNUSED(received_key);
	ARG_UNUSED(key_len);
	return -EACCES;
}

__weak int uds_app_get_periodic_did(uint8_t periodic_did, uint8_t *data_out, size_t *len_out)
{
	ARG_UNUSED(periodic_did);
	ARG_UNUSED(data_out);
	ARG_UNUSED(len_out);
	return -ENOENT;
}

__weak struct nvs_fs *uds_app_get_nvs_context(void)
{
	return NULL;
}

__weak int uds_app_clear_persistent_dtcs(struct nvs_fs *fs, uint32_t dtc_group)
{
	ARG_UNUSED(fs);
	ARG_UNUSED(dtc_group);
	return -EOPNOTSUPP;
}

__weak int uds_app_write_persistent_data(struct nvs_fs *fs, uint16_t nvs_id, const uint8_t *data, size_t len)
{
	ARG_UNUSED(fs);
	ARG_UNUSED(nvs_id);
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	return -EOPNOTSUPP;
}

__weak int uds_app_read_scaling_data(uint16_t did, uint8_t *buf_out, size_t *len_out, size_t max_len)
{
	ARG_UNUSED(did);
	ARG_UNUSED(buf_out);
	ARG_UNUSED(len_out);
	ARG_UNUSED(max_len);
	return -ENOENT;
}
