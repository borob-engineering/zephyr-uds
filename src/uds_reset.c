/**
 * @file uds_reset.c
 * @brief UDS Service 0x11 (ECU Reset) mit verzögertem sys_reboot für Zephyr v4.4.0
 */

#include "uds_reset.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h> /* Zephyr native Reboot API */
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define RESET_DELAY_MS 50 /* Kurze Verzögerung, damit die TX-Antwort den CAN-Transceiver verlassen kann */

static struct k_timer reset_timer;
static int chosen_reset_type = SYS_REBOOT_COLD;

/**
 * @brief Callback wird gefeuert, sobald das ISO-TP Telegramm sicher auf dem Bus war
 */
static void reset_timer_expiry_cb(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    LOG_INF("Triggere System-Reboot via Zephyr Kernel...");
    
    /* Ruft den Hardware-Reset des SoCs auf */
    sys_reboot(chosen_reset_type);
}

void uds_reset_init(void)
{
    k_timer_init(&reset_timer, reset_timer_expiry_cb, NULL);
}

void uds_reset_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                      void (*send_cb)(const uint8_t *, size_t), 
                      void (*nrc_cb)(uint8_t, uint8_t))
{
    uint8_t sid = req[0];
    if (len < 2) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t reset_type = req[1];

    switch (reset_type) {
    case 0x01: /* hardReset */
        chosen_reset_type = SYS_REBOOT_COLD;
        LOG_INF("UDS Hard Reset angefordert.");
        break;

    case 0x03: /* softReset */
        chosen_reset_type = SYS_REBOOT_WARM;
        LOG_INF("UDS Soft Reset angefordert.");
        break;

    default:
        /* Andere Reset-Typen (z.B. keyOffOnReset 0x02) werden in diesem Beispiel nicht unterstützt */
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
        return;
    }

    /* 1. Positive Response aufbauen */
    tx_buf[0] = sid + 0x40;
    tx_buf[1] = reset_type;

    /* 2. Antwort absenden */
    send_cb(tx_buf, 2);

    /* 3. Timer starten, um den Reboot asynchron nach dem TX-Vorgang auszuführen */
    k_timer_start(&reset_timer, K_MSEC(RESET_DELAY_MS), K_NO_WAIT);
}
