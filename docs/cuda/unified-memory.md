# Distributed managed-memory coherence

GVirtuS managed memory spans separate frontend and backend address spaces. CUDA
Unified Memory does not make those hosts coherent; Protocol v2 therefore models
coherence explicitly per session.

The legacy Mapped Pointers Manager becomes the virtual-memory registry. The
Unified Memory Manager becomes a typed coherence service. A managed allocation
tracks frontend/backend authority, dirty epochs, stream association, whether a
kernel may write, and these execution stages:

```text
Idle -> UploadRequired -> ReadyForSubmission -> Submitted
                                                |
                         Idle <- DownloadRequired
```

An upload is required only when the frontend is authoritative and backend work
will read the allocation. Backend submission cannot occur until that upload is
reported complete. A kernel that may write transitions to `DownloadRequired`
only after actual CUDA completion. The response is eligible only after copy-back
restores coherence. Read-only completion can return directly to `Idle`.

Default stream is represented by token zero; explicit streams retain their
virtual object token. Busy allocations cannot be modified or released. Released
allocations become tombstones. Metrics count upload/download bytes and
migrations without using request IDs as high-cardinality labels.

The state machine and example are CPU-tested. Real `cudaMallocManaged`, stream
events, data movement, and kernel callbacks require CUDA-capable integration
tests and are not implemented or claimed here.
