#ifndef UDS_IOCONTROL_H_
#define UDS_IOCONTROL_H_

#include "uds_types.h"

void uds_handle_io_control(uint8_t *req, size_t len, uint8_t *tx_buf, 
                           void (*send_cb)(const uint8_t *, size_t), 
                           void (*nrc_cb)(uint8_t, uint8_t));

#endif /* UDS_IOCONTROL_H_ */
