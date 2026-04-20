#ifndef FOLDEN_ACCESS_LOG_H
#define FOLDEN_ACCESS_LOG_H

#include "folden/folden.h"
#include "folden/folden_types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct folden_access_log folden_access_log_t;

/* Creates an access log that asynchronously writes audit records to a file descriptor 
 * or buffers them for extraction. */
folden_access_log_t *folden_access_log_create(const char *file_path);

void folden_access_log_destroy(folden_access_log_t *log);

/* Records an adopted lane in the access log. */
folden_status_t folden_access_log_record_adopt(folden_access_log_t *log, 
                                               uint32_t lane_id, 
                                               size_t payload_size, 
                                               uint32_t trust_level,
                                               uint64_t timestamp_ms);

/* Records an emitted lane in the access log. */
folden_status_t folden_access_log_record_emit(folden_access_log_t *log, 
                                              uint32_t lane_id, 
                                              size_t payload_size, 
                                              uint64_t timestamp_ms);

/* Records an access denial or validation failure. */
folden_status_t folden_access_log_record_denial(folden_access_log_t *log, 
                                                uint32_t lane_id, 
                                                const char *reason,
                                                uint64_t timestamp_ms);

/* Flushes the buffered records to the underlying storage. */
folden_status_t folden_access_log_flush(folden_access_log_t *log);

/* Truncates the log or rotates it. */
folden_status_t folden_access_log_rotate(folden_access_log_t *log, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_ACCESS_LOG_H */
