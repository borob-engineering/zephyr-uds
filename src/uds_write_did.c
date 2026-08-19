/**
 * @file uds_write_did.c
 * @brief Generischer Service 0x2E (Write Data By Identifier)
 */

#include "uds_write_did.h"
#include "uds_security.h"
#include "uds_session.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <errno.h>

void uds_write_did_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
    if (len == 0 || req == NULL) return;
    uint8_t sid = req[0];

    if (uds_session_get() != UDS_SESSION_EXTENDED) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
        return;
    }

    if (len < 3) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (uds_security_get_status() != UDS_SEC_UNLOCKED) {
        nrc_cb(sid, UDS_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    uint16_t did = ((uint16_t)req[1] << 8) | req[2];

    /* ABSTRAKTION: Aufruf des Applikations-DIDs. Daten beginnen ab Byte-Index 3 */
    int ret = uds_app_write_did(did, &req[3], len - 3);

    if (ret == 0) {
        tx_buf[0] = sid + 0x40;
        tx_buf[1] = req[1];
        tx_buf[2] = req[2];
        send_cb(tx_buf, 3);
    } else if (ret == -EINVAL) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
    } else if (ret == -ENOENT) {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
    } else {
        nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
    }
}
