#ifndef UDS_READ_DTC_H_
#define UDS_READ_DTC_H_

#include "uds_types.h"

void uds_read_dtc_init(void);
void uds_read_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, void (*send_cb)(const uint8_t *, size_t), void (*nrc_cb)(uint8_t, uint8_t));

/* NEU: API zum Zurücksetzen/Löschen der DTCs im RAM */
void uds_read_dtc_clear_all(uint32_t dtc_group);

#endif /* UDS_READ_DTC_H_ */
