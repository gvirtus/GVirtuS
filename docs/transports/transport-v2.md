# Transport v2 contract

Transport v2 is a parallel, transport-neutral interface. It does not alter the
Protocol v1 `Communicator` ABI.

The contract separates small control messages from bulk transfers and exposes
transport capabilities instead of forcing TCP, UCX, and RDMA to emulate one
another. Capability bits report control and bulk support, scatter/gather,
registered host memory, direct GPU memory, and encryption. Endpoint metadata
carries transport, address, connection, and Protocol v2 session identity.
Each implementation also publishes hard limits for control and bulk payloads,
outstanding requests, and scatter/gather entries.

Every operation returns a shared completion handle. Requests move from queued
to in-progress and then exactly once to completed, failed, or cancelled.
Cancellation is explicit and terminal, and failures retain the number of bytes
transferred before the error. Invalid transitions fail closed.

Transfer descriptors distinguish host, pinned-host, and device memory and
carry a transfer ID for correlation with Protocol v2 control frames. Concrete
transports must bound accepted buffers, return backpressure rather than grow
queues without limit, and reject unsupported memory paths unless staging was
explicitly allowed.

Statistics have common counters for control and bulk operations, bytes,
progress calls, stalls, and failures. Transport-specific metrics may extend
these without changing the portable interface.

The first concrete implementation is nonblocking TCP. It provides bounded
control and host-memory bulk queues and is the portable fallback. See
`docs/transports/tcp.md` for its exact behavior and security limitations.

The implementation and tests are CPU-only. They do not claim TLS, UCX,
InfiniBand, RoCE, or GPU-direct support. Those transports must be integrated
and tested independently, including on the required hardware.

## Transport selection

The selection policy filters unavailable transports and checks control/bulk
sizes, outstanding-request and scatter/gather limits, encryption, and memory
capabilities before ranking candidates. A device-memory transfer may use a
registered-host staging path only when the request explicitly permits it.

Priority is the primary rank, followed by an optional soft preference and
capability richness. A hard transport preference never falls back. Encryption
and direct-device requirements also never downgrade silently. Stable input
order breaks otherwise equal ranks, making the decision reproducible.
