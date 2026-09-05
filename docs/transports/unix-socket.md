# Unix-domain socket transport v2

`UnixSocketTransport` provides the transport-v2 contract for local connected
Unix stream sockets. It adopts an already connected descriptor and preserves
connection and Protocol-v2 session identity in endpoint metadata.

The backend uses the same tested nonblocking stream queue as TCP, including
bounded control and host-memory bulk operations, partial I/O, backpressure,
cancellation, failure propagation, idempotent close, and common statistics.
TCP-only socket options are disabled. Send and receive buffer sizes remain
configurable through ordinary socket options.

The transport advertises control and bulk capabilities only. It does not claim
encryption, registered memory, scatter/gather, device-memory access, shared
memory, or zero-copy behavior. Filesystem socket creation, permissions,
authentication, stale-path cleanup, and listener lifecycle belong to the
future endpoint/listener integration and must be secured by the caller in the
meantime.

Tests use an anonymous local socket pair and require neither a GPU nor network
hardware.
