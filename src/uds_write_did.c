/**
 * @file uds_write_did.c
 * @brief UDS Service 0x2E (Write Data By Identifier) mit Session-Validierung für Zephyr v4.4.0
 */

#include "uds_write_did.h"
#include "uds_security.h"
#include "uds_session.h"
#include "uds_data_storage.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

void uds_write_did_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
    if (len == 0 || req == NULL) {
        return;
    }

    uint8_t sid = req[0];

    /* 1. Session-Validierung: Schreiben ist laut Fahrzeug-Spezifikation nur in EXTENDED erlaubt */
    if (uds_session_get() != UDS_SESSION_EXTENDED) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
        return;
    }

    /* 2. Formatprüfung */
    if (len < 3) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    /* 3. Sicherheitsprüfung */
    if (uds_security_get_status() != UDS_SEC_UNLOCKED) {
        nrc_cb(sid, UDS_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    uint16_t did = ((uint16_t)req[1] << 8) | req[2];
    LOG_INF("Write DID Anfrage für: 0x%04X", did);

    if (did == 0xF190) { /* VIN schreiben */
        if (len != (3 + VIN_SIZE)) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }

        if (uds_data_storage_set_vin(&req[3], VIN_SIZE) != 0) {
            nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        LOG_INF("VIN erfolgreich im RAM überschrieben.");

        tx_buf[0] = sid + 0x40;
        tx_buf[1] = req[1];
        tx_buf[2] = req[2];

        send_cb(tx_buf, 3);
    } else {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
    }
}
