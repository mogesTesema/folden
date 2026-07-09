#include <stddef.h>
#include <stdint.h>

#include "folden/folden.h"
#include "replicate.h"

/* Drives the replication/quorum API through a sequence of fuzzer-chosen
 * operations against a small, fixed set of peer IDs, rather than one fixed
 * call order -- the ordering that reaches any bug has to be discovered by
 * mutation, matching how the real state machine would actually be driven
 * over time by many independent peers acking/retrying out of lockstep. */
#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2) {
    return 0;
  }

  folden_replicate_quorum_t *q = folden_replicate_quorum_create();
  folden_replicate_fsm_t *fsm = folden_replicate_fsm_create();
  folden_replicate_fanout_t *f = folden_replicate_fanout_create(q);
  if (!q || !fsm || !f) {
    folden_replicate_fanout_destroy(f);
    folden_replicate_fsm_destroy(fsm);
    folden_replicate_quorum_destroy(q);
    return 0;
  }
  folden_replicate_quorum_attach_fsm(q, fsm);

  for (uint32_t p = 0; p < 4; p++) {
    folden_replicate_quorum_register_peer(q, p);
  }

  size_t off = 0;
  while (off + 2 <= size) {
    uint8_t op = data[off++];
    uint32_t peer = data[off++] % 4;
    const uint8_t *payload = data + off;
    size_t plen = size - off;
    if (plen > 32) {
      plen = 32;
    }

    switch (op % 4) {
      case 0:
        folden_replicate_send_peer(f, fsm, peer, payload, plen);
        break;
      case 1:
        folden_replicate_ack_partial(q, peer);
        break;
      case 2:
        folden_replicate_fsm_retry_tick(fsm, q, peer, payload, plen);
        break;
      case 3:
        folden_replicate_quorum_pending(q);
        break;
    }
  }

  folden_replicate_fanout_destroy(f);
  folden_replicate_fsm_destroy(fsm);
  folden_replicate_quorum_destroy(q);
  return 0;
}

#ifdef __cplusplus
}
#endif
