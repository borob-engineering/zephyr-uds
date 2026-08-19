#ifndef UDS_SECURITY_H_
#define UDS_SECURITY_H_

#include "uds_types.h"

void uds_security_init(void);
uds_security_status_t uds_security_get_status(void);
void uds_security_reset_lock(void);
void uds_security_handle_request(uint8_t *req, size_t len, uint8_t *tx_buf, void (*send_cb)(const uint8_t *, size_t), void (*nrc_cb)(uint8_t, uint8_t));

#endif /* UDS_SECURITY_H_ */
