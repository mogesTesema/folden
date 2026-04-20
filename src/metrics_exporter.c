#include "metrics_exporter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#if defined(_WIN32)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

struct folden_metrics_exporter {
  folden_relay_health_t *health;
  uint16_t port;
  int running;
  int server_fd;
#if !defined(_WIN32)
  pthread_t thread;
#endif
};

folden_metrics_exporter_t *folden_metrics_exporter_create(folden_relay_health_t *health, uint16_t port) {
  if (!health) return NULL;
  
  folden_metrics_exporter_t *exp = calloc(1, sizeof(folden_metrics_exporter_t));
  if (!exp) return NULL;
  
  exp->health = health;
  exp->port = port;
  exp->server_fd = -1;
  return exp;
}

void folden_metrics_exporter_destroy(folden_metrics_exporter_t *exp) {
  if (!exp) return;
  folden_metrics_exporter_stop(exp);
  free(exp);
}

folden_status_t folden_metrics_exporter_handle_request(folden_metrics_exporter_t *exp, 
                                                       const char *request, 
                                                       size_t req_len,
                                                       char *response,
                                                       size_t resp_cap,
                                                       size_t *resp_len) {
  if (!exp || !request || req_len == 0 || !response || !resp_len) return FOLDEN_ERR_BAD_STATE;
  
  // A very crude HTTP request parser to check if they are requesting /metrics
  if (strncmp(request, "GET /metrics ", 13) != 0 && strncmp(request, "GET /metrics\r\n", 14) != 0) {
    const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    size_t l = strlen(not_found);
    if (l < resp_cap) {
      memcpy(response, not_found, l);
      *resp_len = l;
      return FOLDEN_OK;
    }
    return FOLDEN_ERR_NOMEM;
  }
  
  // Format prometheus metrics
  char body[4096];
  size_t w = 0;
  
  folden_percentiles_t ap = folden_health_percentiles(&exp->health->adopt_latency);
  folden_percentiles_t ep = folden_health_percentiles(&exp->health->emit_latency);
  
  w += snprintf(body + w, sizeof(body) - w, "# HELP folden_adopt_latency_us Adopt latency in microseconds\n");
  w += snprintf(body + w, sizeof(body) - w, "# TYPE folden_adopt_latency_us summary\n");
  w += snprintf(body + w, sizeof(body) - w, "folden_adopt_latency_us{quantile=\"0.5\"} %f\n", ap.p50);
  w += snprintf(body + w, sizeof(body) - w, "folden_adopt_latency_us{quantile=\"0.9\"} %f\n", ap.p90);
  w += snprintf(body + w, sizeof(body) - w, "folden_adopt_latency_us{quantile=\"0.99\"} %f\n", ap.p99);
  w += snprintf(body + w, sizeof(body) - w, "folden_adopt_latency_us_sum %f\n", ap.mean * exp->health->adopt_latency.count);
  w += snprintf(body + w, sizeof(body) - w, "folden_adopt_latency_us_count %lu\n", (unsigned long)exp->health->adopt_latency.count);
  
  w += snprintf(body + w, sizeof(body) - w, "# HELP folden_emit_latency_us Emit latency in microseconds\n");
  w += snprintf(body + w, sizeof(body) - w, "# TYPE folden_emit_latency_us summary\n");
  w += snprintf(body + w, sizeof(body) - w, "folden_emit_latency_us{quantile=\"0.5\"} %f\n", ep.p50);
  w += snprintf(body + w, sizeof(body) - w, "folden_emit_latency_us{quantile=\"0.9\"} %f\n", ep.p90);
  w += snprintf(body + w, sizeof(body) - w, "folden_emit_latency_us{quantile=\"0.99\"} %f\n", ep.p99);
  w += snprintf(body + w, sizeof(body) - w, "folden_emit_latency_us_sum %f\n", ep.mean * exp->health->emit_latency.count);
  w += snprintf(body + w, sizeof(body) - w, "folden_emit_latency_us_count %lu\n", (unsigned long)exp->health->emit_latency.count);
  
  w += snprintf(body + w, sizeof(body) - w, "# HELP folden_queue_overflows Total queue overflow events\n");
  w += snprintf(body + w, sizeof(body) - w, "# TYPE folden_queue_overflows counter\n");
  w += snprintf(body + w, sizeof(body) - w, "folden_queue_overflows %lu\n", (unsigned long)exp->health->queue_overflow_count);

  int n = snprintf(response, resp_cap,
                   "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain; version=0.0.4\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n\r\n"
                   "%s", w, body);
                   
  if (n > 0 && (size_t)n < resp_cap) {
    *resp_len = n;
    return FOLDEN_OK;
  }
  
  return FOLDEN_ERR_NOMEM;
}

#if !defined(_WIN32)
static void *exporter_worker(void *arg) {
  folden_metrics_exporter_t *exp = (folden_metrics_exporter_t *)arg;
  
  while (exp->running) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(exp->server_fd, (struct sockaddr *)&client_addr, &client_len);
    
    if (client_fd < 0) {
      continue;
    }
    
    char req_buf[1024];
    ssize_t bytes = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
    if (bytes > 0) {
      req_buf[bytes] = '\0';
      char resp_buf[8192];
      size_t resp_len = 0;
      
      if (folden_metrics_exporter_handle_request(exp, req_buf, bytes, resp_buf, sizeof(resp_buf), &resp_len) == FOLDEN_OK) {
        send(client_fd, resp_buf, resp_len, 0);
      }
    }
    
    close(client_fd);
  }
  
  return NULL;
}
#endif

folden_status_t folden_metrics_exporter_start(folden_metrics_exporter_t *exp) {
#if defined(_WIN32)
  return FOLDEN_ERR_BAD_STATE;
#else
  if (!exp || exp->running) return FOLDEN_ERR_BAD_STATE;
  
  exp->server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (exp->server_fd < 0) return FOLDEN_ERR_BAD_STATE;
  
  int opt = 1;
  setsockopt(exp->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(exp->port);
  
  if (bind(exp->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    close(exp->server_fd);
    exp->server_fd = -1;
    return FOLDEN_ERR_BAD_STATE;
  }
  
  if (listen(exp->server_fd, 5) < 0) {
    close(exp->server_fd);
    exp->server_fd = -1;
    return FOLDEN_ERR_BAD_STATE;
  }
  
  // Set non-blocking so we can cleanly exit
  int flags = fcntl(exp->server_fd, F_GETFL, 0);
  fcntl(exp->server_fd, F_SETFL, flags | O_NONBLOCK);
  
  exp->running = 1;
  if (pthread_create(&exp->thread, NULL, exporter_worker, exp) != 0) {
    exp->running = 0;
    close(exp->server_fd);
    exp->server_fd = -1;
    return FOLDEN_ERR_BAD_STATE;
  }
  
  return FOLDEN_OK;
#endif
}

folden_status_t folden_metrics_exporter_stop(folden_metrics_exporter_t *exp) {
#if defined(_WIN32)
  return FOLDEN_ERR_BAD_STATE;
#else
  if (!exp || !exp->running) return FOLDEN_ERR_BAD_STATE;
  
  exp->running = 0;
  if (exp->server_fd >= 0) {
    close(exp->server_fd);
    exp->server_fd = -1;
  }
  
  pthread_join(exp->thread, NULL);
  return FOLDEN_OK;
#endif
}
