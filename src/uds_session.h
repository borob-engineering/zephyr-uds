#ifndef UDS_SESSION_H_
#define UDS_SESSION_H_

#include "uds_types.h"

void uds_session_init(void);
void uds_session_set(uds_session_type_t new_session);
uds_session_type_t uds_session_get(void);
void uds_session_refresh_timer(void);

#endif /* UDS_SESSION_H_ */
