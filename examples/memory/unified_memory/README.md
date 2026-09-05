# Managed-memory coherence walkthrough

This CPU-only example demonstrates the Protocol v2 distributed managed-memory
state machine. It does not allocate CUDA memory or claim GPU integration.

Configure with `-DGVIRTUS_BUILD_EXAMPLES=ON` and build the
`gvirtus-managed-memory-coherence-example` target. Running it shows the required
pre-launch upload stage followed by the synchronization epoch after simulated
CUDA completion and copy-back.

Expected output:

```text
pre-launch stage=1
restored epoch=1
```
