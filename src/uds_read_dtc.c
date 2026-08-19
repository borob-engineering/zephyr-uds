/**
 * @file uds_read_dtc.c
 * @brief UDS Service 0x19 mit RAM-Speicher und Lösch-Schnittstelle
 */

#include "uds_read_dtc.h"
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
    mock_dtc_server[0].status  = 0x09; /* testFailed | confirmedDTC */

    mock_dtc_server[1].code[0] = 0xC1;
    mock_dtc_server[1].code[1] = 0x00;
    mock_dtc_server[1].code[2] = 0x00;
    mock_dtc_server[1].status  = 0x08; /* confirmedDTC */
}

/* NEU: Setzt die Status-Masken der passenden DTCs zurück */
void uds_read_dtc_clear_all(uint32_t dtc_group)
{
    LOG_INF("DTC Clear API aufgerufen für Gruppe: 0x%06X", dtc_group);

    for (int i = 0; i < MOCK_DTC_COUNT; i++) {
        /* 0xFFFFFF bedeutet 'alle DTC-Gruppen löschen' nach ISO 14229 */
        if (dtc_group == 0xFFFFFF) {
            mock_dtc_server[i].status = 0x00; /* Fehler komplett passivieren/löschen */
        }
        /* Hier könnten optional spezifischere Gruppenfilter (z.B. Body, Powertrain) greifen */
    }
}

void uds_read_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                         void (*send_cb)(const uint8_t *, size_t), 
                         void (*nrc_cb)(uint8_t, uint8_t))
{
    uint8_t sid = req[0];

    if (len < 3) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t sub_function = req[1];
    if (sub_function != 0x02) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
        return;
    }

    uint8_t client_status_mask = req[2];
    size_t tx_idx = 0;

    tx_buf[tx_idx++] = sid + 0x40;
    tx_buf[tx_idx++] = sub_function;
    tx_buf[tx_idx++] = DTC_STATUS_AVAILABILITY_MASK;

    for (int i = 0; i < MOCK_DTC_COUNT; i++) {
        /* Nur DTCs anhängen, deren Status-Bits mit der angeforderten Maske übereinstimmen und aktiv (!=0) sind */
        if (mock_dtc_server[i].status != 0x00 && (mock_dtc_server[i].status & client_status_mask) != 0) {
            
            if ((tx_idx + 4) >= UDS_BUFF_SIZE) {
                LOG_ERR("UDS TX Buffer Limit erreicht!");
                break;
            }

            tx_buf[tx_idx++] = mock_dtc_server[i].code[0];
            tx_buf[tx_idx++] = mock_dtc_server[i].code[1];
            tx_buf[tx_idx++] = mock_dtc_server[i].code[2];
            tx_buf[tx_idx++] = mock_dtc_server[i].status;
        }
    }

    send_cb(tx_buf, tx_idx);
}
