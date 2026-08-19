/**
 * @file uds_routine.c
 * @brief Asynchrone Routine Control Engine (0x31) mit Sitzungsüberprüfung
 */

#include "uds_routine.h"
#include "uds_session.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define ROUTINE_ID_ERASE_MEMORY 0xFF00

static volatile routine_status_t erase_routine_status = ROUTINE_IDLE;
static uint8_t routine_exit_info = 0x00;
static struct k_work routine_work;

static void routine_worker_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    LOG_INF("Routine 0xFF00: Lösche Flash im Hintergrund...");
    k_msleep(3000); 
    erase_routine_status = ROUTINE_COMPLETED;
    routine_exit_info = 0x00;
    LOG_INF("Routine 0xFF00 beendet.");
}

void uds_routine_init(void)
{
    k_work_init(&routine_work, routine_worker_handler);
}

void uds_routine_handle_control(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                void (*send_cb)(const uint8_t *, size_t), 
                                void (*nrc_cb)(uint8_t, uint8_t))
{
    if (len == 0 || req == NULL) {
        return;
    }

    uint8_t sid = req[0];

    /* Session-Validierung: Routinensteuerung ist in der Standard-Sitzung untersagt */
    if (uds_session_get() == UDS_SESSION_DEFAULT) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
        return;
    }

    if (len < 4) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t sub_function = req[1];
    uint16_t routine_id = ((uint16_t)req[2] << 8) | req[3];

    if (routine_id != ROUTINE_ID_ERASE_MEMORY) {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    switch (sub_function) {
    case 0x01: /* startRoutine */
        if (erase_routine_status == ROUTINE_RUNNING) {
            nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
            return;
        }
        erase_routine_status = ROUTINE_RUNNING;
        k_work_submit(&routine_work);

        tx_buf[0] = sid + 0x40;
        tx_buf[1] = sub_function;
        tx_buf[2] = req[2];
        tx_buf[3] = req[3];
        tx_buf[4] = 0x01; 
        send_cb(tx_buf, 5);
        break;

    case 0x03: /* requestRoutineResults */
        tx_buf[0] = sid + 0x40;
        tx_buf[1] = sub_function;
        tx_buf[2] = req[2];
        tx_buf[3] = req[3];
        tx_buf[4] = (erase_routine_status == ROUTINE_RUNNING) ? 0x01 : 
                    (erase_routine_status == ROUTINE_COMPLETED) ? 0x00 : 0x02;
        tx_buf[5] = routine_exit_info;
        send_cb(tx_buf, 6);
        break;

    default:
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
        break;
    }
}
