/**
 * @file uds_security.c
 * @brief Generischer Security-Access Treiber mit permanenter NVS-Sicherung gegen Brute-Force-Angriffe
 */

#include "uds_security.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kvss/nvs.h> /* KORREKTUR: Modernes Zephyr 4.4.0 KVSS-Header anstelle von fs/nvs.h */
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define SECURITY_SEED_SIZE   CONFIG_UDS_SEC_SEED_SIZE
#define MAX_FAILED_ATTEMPTS  CONFIG_UDS_SEC_MAX_FAILED_ATTEMPTS
#define LOCKOUT_TIME_MS      CONFIG_UDS_SEC_LOCKOUT_TIME_MS

/* NVS Konfigurations-IDs */
#define NVS_DTC_COUNTER_ID   1
#define NVS_LOCKOUT_STATE_ID 2

static uds_security_status_t security_status = UDS_SEC_LOCKED;
static uint8_t generated_seed[SECURITY_SEED_SIZE];
static uint8_t failed_attempts_counter = 0;
static struct k_timer lockout_timer;

static const struct device *entropy_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
static const struct device *flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
static struct nvs_fs fs;

static void lockout_timer_expiry_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	security_status = UDS_SEC_LOCKED;
	failed_attempts_counter = 0;
	
	/* Flash-Zustand nach Ablauf der Zeit wieder zurücksetzen */
	uint8_t zero = 0;
	(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &zero, sizeof(zero));
	(void)nvs_write(&fs, NVS_LOCKOUT_STATE_ID, &zero, sizeof(zero));
	
	LOG_INF("Anti-Brute-Force Sperre abgelaufen. Flash-Zähler zurückgesetzt.");
}

void uds_security_init(void)
{
	int ret;
	struct flash_pages_info info;

	k_timer_init(&lockout_timer, lockout_timer_expiry_cb, NULL);

	if (!device_is_ready(flash_dev)) {
		LOG_ERR("Flash Controller für NVS nicht bereit!");
		return;
	}

	/* Konfiguriere das NVS-Dateisystem auf der letzten verfügbaren Page des STM32 */
	fs.flash_device = flash_dev;
	ret = flash_get_page_info_by_idx(flash_dev, flash_get_page_count(flash_dev) - 1, &info);
	if (ret == 0) {
		fs.offset = info.start_offset;
		fs.sector_size = info.size;
		fs.sector_count = 1;
	} else {
		LOG_ERR("Konnte Flash-Page-Info nicht lesen");
		return;
	}

	ret = nvs_mount(&fs);
	if (ret != 0) {
		LOG_ERR("NVS Mount fehlgeschlagen: %d", ret);
		return;
	}

	/* Lese vorherige Fehlerzustände aus dem Speicher */
	uint8_t stored_lockout = 0;
	ret = nvs_read(&fs, NVS_LOCKOUT_STATE_ID, &stored_lockout, sizeof(stored_lockout));
	if (ret > 0 && stored_lockout == 1) {
		/* REBOOT-REGEL: Reaktivierung der Sperre, falls das Gerät im Lockout resettet wurde */
		security_status = UDS_SEC_BRUTE_FORCE_LOCKOUT;
		failed_attempts_counter = MAX_FAILED_ATTEMPTS;
		k_timer_start(&lockout_timer, K_MSEC(LOCKOUT_TIME_MS), K_NO_WAIT);
		LOG_WRN("Brute-Force Lockout aus Flash wiederhergestellt! Schnittstelle bleibt gesperrt.");
		return;
	}

	ret = nvs_read(&fs, NVS_DTC_COUNTER_ID, &failed_attempts_counter, sizeof(failed_attempts_counter));
	if (ret <= 0) {
		failed_attempts_counter = 0;
	} else if (failed_attempts_counter > 0) {
		LOG_INF("UDS Security: %d gespeicherte Fehlversuche aus Flash geladen.", failed_attempts_counter);
	}
}

uds_security_status_t uds_security_get_status(void)
{
	return security_status;
}

void uds_security_reset_lock(void)
{
	if (security_status != UDS_SEC_BRUTE_FORCE_LOCKOUT) {
		security_status = UDS_SEC_LOCKED;
	}
}

void uds_security_handle_request(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	if (len < 2) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	if (security_status == UDS_SEC_BRUTE_FORCE_LOCKOUT) {
		nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
		return;
	}

	uint8_t sub_function = req[1];

	if (sub_function == 0x01) {
		if (len != 2) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}

		if (security_status == UDS_SEC_UNLOCKED) {
			memset(generated_seed, 0, SECURITY_SEED_SIZE);
		} else if (security_status != UDS_SEC_SEED_REQUESTED) {
			if (device_is_ready(entropy_dev)) {
				if (entropy_get_entropy(entropy_dev, generated_seed, SECURITY_SEED_SIZE) < 0) {
					uint32_t uptime = k_uptime_get_32();
					for (int i = 0; i < SECURITY_SEED_SIZE; i++) generated_seed[i] = (uint8_t)(uptime >> (i * 8));
				}
			} else {
				uint32_t uptime = k_uptime_get_32();
				for (int i = 0; i < SECURITY_SEED_SIZE; i++) generated_seed[i] = (uint8_t)(uptime >> (i * 8));
			}
			security_status = UDS_SEC_SEED_REQUESTED;
		}

		tx_buf[0] = sid + 0x40;
		tx_buf[1] = sub_function;
		memcpy(&tx_buf[2], generated_seed, SECURITY_SEED_SIZE);
		send_cb(tx_buf, 2 + SECURITY_SEED_SIZE);

	} else if (sub_function == 0x02) {
		if (len != (2 + SECURITY_SEED_SIZE)) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}

		if (security_status != UDS_SEC_SEED_REQUESTED) {
			nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
			return;
		}

		uint32_t received_key = ((uint32_t)req[2] << 24) | ((uint32_t)req[3] << 16) | 
		                        ((uint32_t)req[4] << 8)  | (uint32_t)req[5];
                                
		uint32_t expected_key = uds_app_calculate_key(generated_seed, SECURITY_SEED_SIZE);

		if (received_key == expected_key) {
			security_status = UDS_SEC_UNLOCKED;
			failed_attempts_counter = 0;
			
			uint8_t zero = 0;
			(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &zero, sizeof(zero));
			
			tx_buf[0] = sid + 0x40;
			tx_buf[1] = sub_function;
			send_cb(tx_buf, 2);
		} else {
			failed_attempts_counter++;
			
			/* Fehlerzähler sofort persistent im Flash wegsichern */
			(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &failed_attempts_counter, sizeof(failed_attempts_counter));

			if (failed_attempts_counter >= MAX_FAILED_ATTEMPTS) {
				security_status = UDS_SEC_BRUTE_FORCE_LOCKOUT;
				
				/* Lockout-Zustand persistent im Flash setzen */
				uint8_t lockout_active = 1;
				(void)nvs_write(&fs, NVS_LOCKOUT_STATE_ID, &lockout_active, sizeof(lockout_active));
				
				k_timer_start(&lockout_timer, K_MSEC(LOCKOUT_TIME_MS), K_NO_WAIT);
				nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
			} else {
				security_status = UDS_SEC_LOCKED;
				nrc_cb(sid, UDS_NRC_INVALID_KEY);
			}
		}
	} else {
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
	}
}
