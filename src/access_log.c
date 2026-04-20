#include "access_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 4096

struct folden_access_log {
  FILE *file;
  char *file_path;
  char buffer[BUFFER_SIZE];
  size_t buf_len;
};

folden_access_log_t *folden_access_log_create(const char *file_path) {
  if (!file_path) return NULL;
  
  folden_access_log_t *log = calloc(1, sizeof(folden_access_log_t));
  if (!log) return NULL;
  
  log->file_path = strdup(file_path);
  if (!log->file_path) {
    free(log);
    return NULL;
  }
  
  log->file = fopen(log->file_path, "a");
  if (!log->file) {
    free(log->file_path);
    free(log);
    return NULL;
  }
  
  return log;
}

void folden_access_log_destroy(folden_access_log_t *log) {
  if (!log) return;
  folden_access_log_flush(log);
  if (log->file) fclose(log->file);
  free(log->file_path);
  free(log);
}

static folden_status_t buffer_write(folden_access_log_t *log, const char *msg, size_t len) {
  if (log->buf_len + len >= BUFFER_SIZE) {
    folden_access_log_flush(log);
  }
  
  if (len >= BUFFER_SIZE) {
    if (log->file) fwrite(msg, 1, len, log->file);
  } else {
    memcpy(log->buffer + log->buf_len, msg, len);
    log->buf_len += len;
  }
  
  return FOLDEN_OK;
}

folden_status_t folden_access_log_record_adopt(folden_access_log_t *log, 
                                               uint32_t lane_id, 
                                               size_t payload_size, 
                                               uint32_t trust_level,
                                               uint64_t timestamp_ms) {
  if (!log) return FOLDEN_ERR_BAD_STATE;
  
  char buf[256];
  int n = snprintf(buf, sizeof(buf), "[%llu] ADOPT lane=%u size=%zu trust=%u\n",
                   (unsigned long long)timestamp_ms, lane_id, payload_size, trust_level);
  if (n > 0) {
    return buffer_write(log, buf, (size_t)n);
  }
  return FOLDEN_ERR_BAD_STATE;
}

folden_status_t folden_access_log_record_emit(folden_access_log_t *log, 
                                              uint32_t lane_id, 
                                              size_t payload_size, 
                                              uint64_t timestamp_ms) {
  if (!log) return FOLDEN_ERR_BAD_STATE;
  
  char buf[256];
  int n = snprintf(buf, sizeof(buf), "[%llu] EMIT lane=%u size=%zu\n",
                   (unsigned long long)timestamp_ms, lane_id, payload_size);
  if (n > 0) {
    return buffer_write(log, buf, (size_t)n);
  }
  return FOLDEN_ERR_BAD_STATE;
}

folden_status_t folden_access_log_record_denial(folden_access_log_t *log, 
                                                uint32_t lane_id, 
                                                const char *reason,
                                                uint64_t timestamp_ms) {
  if (!log || !reason) return FOLDEN_ERR_BAD_STATE;
  
  char buf[512];
  int n = snprintf(buf, sizeof(buf), "[%llu] DENY lane=%u reason=\"%s\"\n",
                   (unsigned long long)timestamp_ms, lane_id, reason);
  if (n > 0) {
    return buffer_write(log, buf, (size_t)n);
  }
  return FOLDEN_ERR_BAD_STATE;
}

folden_status_t folden_access_log_flush(folden_access_log_t *log) {
  if (!log) return FOLDEN_ERR_BAD_STATE;
  if (log->buf_len > 0 && log->file) {
    fwrite(log->buffer, 1, log->buf_len, log->file);
    fflush(log->file);
    log->buf_len = 0;
  }
  return FOLDEN_OK;
}

folden_status_t folden_access_log_rotate(folden_access_log_t *log, const char *new_path) {
  if (!log || !new_path) return FOLDEN_ERR_BAD_STATE;
  
  folden_access_log_flush(log);
  if (log->file) fclose(log->file);
  
  free(log->file_path);
  log->file_path = strdup(new_path);
  if (!log->file_path) return FOLDEN_ERR_NOMEM;
  
  log->file = fopen(log->file_path, "a");
  if (!log->file) return FOLDEN_ERR_BAD_STATE;
  
  return FOLDEN_OK;
}
