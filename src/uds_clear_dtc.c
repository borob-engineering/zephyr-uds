/**
 * @file uds_clear_dtc.c
 * @brief UDS Service 0x14 (Clear Diagnostic Information) mit zyklischem NRC 0x78 Handling
 */

#include "uds_clear_dtc.h"
#include "uds_read_dtc.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define NRC78_PERIOD_MS 20

static struct k_work clear_dtc_work;
static struct k_timer nrc78_timer;

static uint32_t target_dtc_group = 0;
static uint8_t local_tx_buf;
static uint8_t stored_sid = 0x14;

/* Atomares Flag zur Vermeidung von Race Conditions bei der Timer-Abschaltung */
static volatile bool worker_done = false;

static void (*stored_send_cb)(const uint8_t *, size_t) = NULL;
static void (*stored_nrc_cb)(uint8_t, uint8_t) = NULL;

/**
 * @brief One-Shot Timer-Callback. Zieht sich selbst nur bei Bedarf neu auf.
 */
static void nrc78_timer_expiry_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	
	/* Wenn der Worker fertig ist, brechen wir ab und planen keinen neuen Durchlauf */
	if (worker_done) {
		return;
	}

	if (stored_nrc_cb != NULL) {
		/* Sendet das gesetzlich geforderte NRC 0x78 */
		stored_nrc_cb(stored_sid, UDS_NRC_RESPONSE_PENDING);
	}

	/* Nur wenn der Worker immer noch läuft, planen wir das nächste Feuern in 20ms */
	if (!worker_done) {
		k_timer_start(&nrc78_timer, K_MSEC(NRC78_PERIOD_MS), K_NO_WAIT);
	}
}

/**
 * @brief Asynchroner Flash-Lösch-Worker (Läuft parallel in der System-Workqueue)
 */
static void clear_dtc_worker_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	LOG_INF("Hintergrund-Flash-Loeschung aktiv...");
	
	/* Simuliere reale zeitintensive Flash-Hardware-Löschung (z.B. 1200 Millisekunden) */
	k_msleep(1200); 

	/* Interne DTC-RAM-Datenbank bereinigen */
	uds_read_dtc_clear_all(target_dtc_group);

	/* Zerstörungsfreies Abschalten: Flag sperren und Timer stoppen */
	worker_done = true;
	k_timer_stop(&nrc78_timer);

	/* Dem CAN-Controller eine Atempause geben, um Leitungs-Puffer zu leeren */
	k_msleep(10);

	LOG_INF("Loeschvorgang beendet. Sende finale positive UDS-Antwort.");

	if (stored_send_cb != NULL) {
		local_tx_buf = 0x54; /* SID 0x14 + 0x40 */
		stored_send_cb(&local_tx_buf, 1);
	}
}

void uds_clear_dtc_init(void)
{
	k_work_init(&clear_dtc_work, clear_dtc_worker_handler);
	/* Initialisierung des Kernel-Timers */
	k_timer_init(&nrc78_timer, nrc78_timer_expiry_cb, NULL);
}

void uds_clear_dtc_handle(uint8_t *req, size_t len, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
	/* RE-KORREKTUR: Nutzung der originalen Array-Indizes */
	uint8_t sid = req[0];
	
	if (len != 4) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	if (k_work_is_pending(&clear_dtc_work)) {
		nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
		return;
	}

	/* RE-KORREKTUR: Nutzung der originalen Array-Indizes für die DTC-Gruppe */
	target_dtc_group = ((uint32_t)req[1] << 16) | 
	                   ((uint32_t)req[2] << 8)  | 
	                   ((uint32_t)req[3]);

	stored_send_cb = send_cb;
	stored_nrc_cb = nrc_cb;
	stored_sid = sid;

	/* Flag für den neuen Durchlauf zurücksetzen */
	worker_done = false;

	/* Start des Timers als One-Shot nach 20ms */
	k_timer_start(&nrc78_timer, K_MSEC(NRC78_PERIOD_MS), K_NO_WAIT);

	/* Schiebe den eigentlichen Lösch-Task in die parallele Abarbeitung */
	k_work_submit(&clear_dtc_work);
}
