/**
 * @file uds_iocontrol.c
 * @brief Generischer Service 0x2F (Input Output Control By Identifier)
 */

#include "uds_iocontrol.h"
#include "uds_session.h"
#include "uds_security.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <errno.h>

void uds_handle_io_control(uint8_t *req, size_t len, uint8_t *tx_buf, 
                           void (*send_cb)(const uint8_t *, size_t), 
                           void (*nrc_cb)(uint8_t, uint8_t))
{
    if (len == 0 || req == NULL) return;
    uint8_t sid = req[0];

    if (uds_session_get() != UDS_SESSION_EXTENDED) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
        return;
    }

    if (uds_security_get_status() != UDS_SEC_UNLOCKED) {
        nrc_cb(sid, UDS_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    if (len < 4) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint16_t data_id = ((uint16_t)req[1] << 8) | req[2];
    uint8_t control_param = req[3];
    uint8_t output_status = 0;

    /* ABSTRAKTION: Aufruf des Applikations-IOs. Optionale Zusatzdaten ab Byte-Index 4 */
    int ret = uds_app_io_control(data_id, control_param, &req[4], len - 4, &output_status);

    if (ret == 0) {
        tx_buf[0] = sid + 0x40;
        tx_buf[1] = req[1];
        tx_buf[2] = req[2];
        tx_buf[3] = control_param;
        tx_buf[4] = output_status;
        send_cb(tx_buf, 5);
    } else if (ret == -EINVAL) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
    } else if (ret == -ENOENT) {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
    } else {
        nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
    }
}
