#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capsule_wire.h"
#include "folden/folden.h"
#include "../src/relay_health.h"
#include "../src/capsule_mux.h"
#include "../src/flow_control.h"
#include "../src/relay_session.h"

static uint8_t *read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    perror("fopen");
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  uint8_t *buf = malloc(sz > 0 ? (size_t)sz : 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    fprintf(stderr, "read failed: %s\n", path);
    free(buf);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *out_len = (size_t)sz;
  return buf;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s <command> [args...]\n"
          "commands:\n"
          "  inspect <capsule.bin>                 decode and summarize a capsule\n"
          "  stats <capsule.bin>                    adopt + emit, then print per-lane stats\n"
          "  routes <capsule.bin> <next_hop:cost>*  adopt, register routes, print lookups\n"
          "  configure <capsule.bin> <conf.txt>     adopt, apply config, tick, print journal state\n"
          "  health <capsule.bin>                  simulate traffic and dump health metrics\n"
          "  mux <capsule1.bin> [capsule2.bin...]  multiplex capsules into a stream and unpack\n",
          argv0);
}

static int cmd_inspect(const char *path) {
  size_t len = 0;
  uint8_t *buf = read_file(path, &len);
  if (!buf) {
    return 1;
  }
  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, len, &cap);
  if (st != FOLDEN_OK) {
    fprintf(stderr, "decode failed: %d\n", (int)st);
    free(buf);
    return 1;
  }
  size_t lanes = folden_capsule_lane_count(cap);
  printf("lanes=%zu\n", lanes);
  for (size_t i = 0; i < lanes; i++) {
    uint32_t lane_id = folden_capsule_lane_id(cap, i);
    printf("  lane %u", lane_id);
    if (folden_capsule_lane_has_vault_gen(cap, i)) {
      printf(" vault_gen=%u", folden_capsule_lane_vault_gen(cap, i));
    }
    printf("\n");
  }
  folden_capsule_free(cap);
  free(buf);
  return 0;
}

static int cmd_stats(const char *path) {
  size_t len = 0;
  uint8_t *buf = read_file(path, &len);
  if (!buf) {
    return 1;
  }
  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, len, &cap);
  if (st != FOLDEN_OK) {
    fprintf(stderr, "decode failed: %d\n", (int)st);
    free(buf);
    return 1;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_relay_adopt(relay, cap);
  uint8_t *out = NULL;
  size_t out_len = 0;
  folden_emit_capsule(cap, relay, &out, &out_len);
  free(out);

  size_t lanes = folden_capsule_lane_count(cap);
  for (size_t i = 0; i < lanes; i++) {
    uint32_t lane_id = folden_capsule_lane_id(cap, i);
    folden_lane_stats_t stats;
    if (folden_relay_lane_stats(relay, lane_id, &stats) == FOLDEN_OK) {
      printf("lane %u: adopts=%llu releases=%llu emits=%llu bytes_in=%llu bytes_out=%llu\n",
             lane_id, (unsigned long long)stats.adopt_count,
             (unsigned long long)stats.release_count,
             (unsigned long long)stats.emit_count,
             (unsigned long long)stats.bytes_in,
             (unsigned long long)stats.bytes_out);
    }
  }

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(buf);
  return 0;
}

static int cmd_routes(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "routes requires a capsule path\n");
    return 2;
  }
  size_t len = 0;
  uint8_t *buf = read_file(argv[2], &len);
  if (!buf) {
    return 1;
  }
  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(buf, len, &cap);
  if (st != FOLDEN_OK) {
    fprintf(stderr, "decode failed: %d\n", (int)st);
    free(buf);
    return 1;
  }

  folden_relay_t *relay = folden_relay_create();
  folden_relay_adopt(relay, cap);

  size_t lanes = folden_capsule_lane_count(cap);
  size_t route_arg = 3;
  for (size_t i = 0; i < lanes && route_arg < (size_t)argc; i++, route_arg++) {
    uint32_t lane_id = folden_capsule_lane_id(cap, i);
    uint32_t next_hop = 0, cost = 0;
    if (sscanf(argv[route_arg], "%u:%u", &next_hop, &cost) == 2) {
      folden_relay_route_add(relay, lane_id, next_hop, cost);
    }
  }

  for (size_t i = 0; i < lanes; i++) {
    uint32_t lane_id = folden_capsule_lane_id(cap, i);
    uint32_t hop = 0;
    if (folden_relay_route_lookup(relay, lane_id, &hop) == FOLDEN_OK) {
      printf("lane %u -> next_hop %u\n", lane_id, hop);
    } else {
      printf("lane %u -> (no route)\n", lane_id);
    }
  }

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(buf);
  return 0;
}

static int cmd_configure(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "configure requires a capsule path and a config path\n");
    return 2;
  }
  size_t cap_len = 0;
  uint8_t *cap_buf = read_file(argv[2], &cap_len);
  if (!cap_buf) {
    return 1;
  }
  size_t conf_len = 0;
  uint8_t *conf_buf = read_file(argv[3], &conf_len);
  if (!conf_buf) {
    free(cap_buf);
    return 1;
  }

  folden_capsule_t *cap = NULL;
  folden_status_t st = folden_decode_buffer(cap_buf, cap_len, &cap);
  if (st != FOLDEN_OK) {
    fprintf(stderr, "decode failed: %d\n", (int)st);
    free(cap_buf);
    free(conf_buf);
    return 1;
  }

  folden_relay_t *relay = folden_relay_create();
  int error_line = 0;
  folden_status_t conf_st =
      folden_relay_configure(relay, (const char *)conf_buf, conf_len, &error_line);
  if (conf_st != FOLDEN_OK) {
    fprintf(stderr, "config error on line %d\n", error_line);
    folden_relay_destroy(relay);
    folden_capsule_free(cap);
    free(cap_buf);
    free(conf_buf);
    return 1;
  }

  folden_relay_adopt(relay, cap);
  folden_relay_scheduler_tick(relay);
  printf("configured and ticked ok, lanes=%zu\n", folden_capsule_lane_count(cap));

  folden_relay_destroy(relay);
  folden_capsule_free(cap);
  free(cap_buf);
  free(conf_buf);
  return 0;
}

static int cmd_health(const char *path) {
  size_t len = 0;
  uint8_t *buf = read_file(path, &len);
  if (!buf) return 1;

  folden_capsule_t *cap = NULL;
  if (folden_decode_buffer(buf, len, &cap) != FOLDEN_OK) {
    free(buf);
    return 1;
  }

  folden_relay_health_t *health = folden_health_create(50);
  folden_flow_ctrl_t *flow = folden_flow_create(10.0, 50.0, 0);

  size_t lanes = folden_capsule_lane_count(cap);
  for (size_t i = 0; i < lanes; i++) {
    uint32_t lane_id = folden_capsule_lane_id(cap, i);
    if (folden_flow_check(flow, lane_id, 0) == FOLDEN_OK) {
      folden_flow_consume(flow, lane_id);
      folden_health_record_adopt(health, lane_id, 120 + i * 5); // dummy latency
      folden_health_record_emit(health, lane_id, 40 + i * 2);
    }
  }

  folden_health_record_queue_overflow(health);
  folden_health_tick(health);
  folden_flow_tick(flow);

  char dump_buf[2048];
  folden_health_dump(health, dump_buf, sizeof(dump_buf));
  printf("--- Health Dump ---\n%s", dump_buf);

  folden_health_destroy(health);
  folden_flow_destroy(flow);
  folden_capsule_free(cap);
  free(buf);
  return 0;
}

static void mux_cb(const uint8_t *wire, size_t wire_len, void *user) {
  size_t *count = (size_t *)user;
  (*count)++;
  printf("demuxed capsule %zu: %zu bytes\n", *count, wire_len);
}

static int cmd_mux(int argc, char **argv) {
  if (argc < 3) return 1;
  
  size_t count = argc - 2;
  const uint8_t **payloads = malloc(count * sizeof(uint8_t *));
  size_t *lengths = malloc(count * sizeof(size_t));
  
  for (size_t i = 0; i < count; i++) {
    uint8_t *b = read_file(argv[2 + i], &lengths[i]);
    if (!b) {
      for (size_t j = 0; j < i; j++) free((void *)payloads[j]);
      free(payloads); free(lengths);
      return 1;
    }
    payloads[i] = b;
  }

  folden_mux_frame_t frame;
  folden_status_t st = folden_mux_pack(payloads, lengths, count, &frame);
  if (st != FOLDEN_OK) {
    fprintf(stderr, "mux pack failed\n");
    return 1;
  }
  printf("packed %zu capsules into %zu bytes\n", count, frame.len);

  folden_mux_stream_t *ms = folden_mux_stream_create();
  size_t demux_count = 0;
  folden_mux_stream_feed(ms, frame.buf, frame.len, mux_cb, &demux_count);
  
  folden_mux_stream_destroy(ms);
  folden_mux_frame_free(&frame);
  for (size_t i = 0; i < count; i++) free((void *)payloads[i]);
  free(payloads); free(lengths);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "inspect") == 0 && argc == 3) {
    return cmd_inspect(argv[2]);
  }
  if (strcmp(argv[1], "stats") == 0 && argc == 3) {
    return cmd_stats(argv[2]);
  }
  if (strcmp(argv[1], "routes") == 0) {
    return cmd_routes(argc, argv);
  }
  if (strcmp(argv[1], "configure") == 0) {
    return cmd_configure(argc, argv);
  }
  if (strcmp(argv[1], "health") == 0 && argc == 3) {
    return cmd_health(argv[2]);
  }
  if (strcmp(argv[1], "mux") == 0) {
    return cmd_mux(argc, argv);
  }
  usage(argv[0]);
  return 2;
}
