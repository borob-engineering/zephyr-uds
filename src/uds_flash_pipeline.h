#ifndef UDS_FLASH_PIPELINE_H_
#define UDS_FLASH_PIPELINE_H_

#include "uds_types.h"

void uds_flash_pipeline_init(void);

void uds_handle_request_download(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t));

void uds_handle_transfer_data(uint8_t *req, size_t len, uint8_t *tx_buf, 
                              void (*send_cb)(const uint8_t *, size_t), 
                              void (*nrc_cb)(uint8_t, uint8_t));

void uds_handle_request_transfer_exit(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                      void (*send_cb)(const uint8_t *, size_t), 
                                      void (*nrc_cb)(uint8_t, uint8_t));

#endif /* UDS_FLASH_PIPELINE_H_ */
