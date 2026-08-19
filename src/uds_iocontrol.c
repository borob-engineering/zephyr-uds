/**
 * @file uds_iocontrol.c
 * @brief Implementierung von Service 0x2F (Input Output Control By Identifier) - Stellgliedtest
 */

#include "uds_iocontrol.h"
#include "uds_session.h"
#include "uds_security.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define DID_STATUS_LED 0x0123
static bool led_override_active = false;
static uint8_t led_mock_state = 0;

void uds_handle_io_control(uint8_t *req, size_t len, uint8_t *tx_buf, 
                           void (*send_cb)(const uint8_t *, size_t), 
                           void (*nrc_cb)(uint8_t, uint8_t))
{
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

    if (data_id != DID_STATUS_LED) {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    tx_buf[0] = sid + 0x40;
    tx_buf[1] = req[1];
    tx_buf[2] = req[2];
    tx_buf[3] = control_param;

    if (control_param == 0x00) { /* returnControlToECU */
        led_override_active = false;
        LOG_INF("IO-Control: Kontrolle über Status-LED an die ECU zurückgegeben.");
        tx_buf[4] = led_mock_state;
        send_cb(tx_buf, 5);
    } else if (control_param == 0x03) { /* shortTermAdjustment */
        if (len < 5) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }
        led_override_active = true;
        led_mock_state = req[4];
        LOG_INF("IO-Control: Status-LED manuell überschrieben. Zustand: %u", led_mock_state);
        tx_buf[4] = led_mock_state;
        send_cb(tx_buf, 5);
    } else {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }
}
