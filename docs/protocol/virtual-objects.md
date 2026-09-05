# Protocol v2 virtual CUDA objects

Protocol v2 represents CUDA objects with opaque 64-bit tokens. Backend CUDA
handles and addresses remain inside backend session state and are never used as
wire identities.

Each token contains a session namespace, an object-type tag, and a monotonically
allocated sequence. The type tag permits validation without interpreting the
backend handle. Initial object kinds cover streams, events, arrays, mipmapped
arrays, graphs, graph executions, modules, functions, textures, surfaces,
library handles, and descriptors.

## Lifecycle rules

- Session namespaces must be non-zero.
- Backend object values must be non-zero and remain backend-only.
- Each operation supplies its expected object type.
- Cross-session and wrong-type handles are rejected before lookup.
- Released tokens become tombstones and cannot be reused.
- Duplicate destruction reports a stale handle.
- Active objects are bounded per session.
- Session teardown drains all remaining records exactly once so their backend
  resources can be destroyed deterministically.

Registry methods are synchronized because future Protocol v2 sessions may have
multiple requests in flight. CUDA-specific create, destroy, stream-ordering,
and asynchronous-completion behavior will be layered over this registry rather
than embedded in token parsing.
