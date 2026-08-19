#ifndef UDS_ROUTINE_H_
#define UDS_ROUTINE_H_

#include "uds_types.h"

void uds_routine_init(void);
void uds_routine_handle_control(uint8_t *req, size_t len, 
                                void (*send_cb)(const uint8_t *, size_t), 
                                void (*nrc_cb)(uint8_t, uint8_t));

#endif /* UDS_ROUTINE_H_ */
