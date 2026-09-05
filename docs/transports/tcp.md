# TCP transport

TCP is GVirtuS's universal Protocol v1 compatibility transport. It builds
without RDMA headers or libraries; enable the legacy RDMA communicator
separately with `-DGVIRTUS_ENABLE_RDMA=ON`.

## Runtime behavior

The communicator enables keepalive and disables Nagle's algorithm. Reads and
writes continue across interrupted and partial system calls. `Close()` performs
an idempotent full-duplex shutdown, and destruction closes any socket still
owned by the communicator.

Protocol framing remains the responsibility of the existing encoder and
decoder. This transport does not interpret CUDA operations.

## Validation

Configure with `-DGVIRTUS_BUILD_TESTS=ON`, build `tcp-communicator-test`, and
use `-DGVIRTUS_BUILD_CUDA_PLUGINS=OFF` on hosts without CUDA, then run `ctest`.
The regression test uses a local socket pair, requires no GPU, and
checks fragmented reads, complete writes, and repeated close operations.

## Protocol v2 transport

The separate `TcpTransport` implements the transport-v2 contract without
changing the Protocol v1 communicator ABI. It adopts a connected socket, sets
nonblocking mode, enables keepalive and `TCP_NODELAY` where the socket family
supports them, and optionally configures send and receive socket buffers.

Control and bulk operations use bounded FIFO queues. Send payloads are copied
into transport-owned storage; receive buffers remain caller-owned until their
request reaches a terminal state. `Progress()` handles partial transfers,
would-block results, interruption, peer shutdown, cancellation, and failure of
all pending work after a connection error. Statistics report accepted
operations, bytes, progress stalls, and failures.

TCP advertises host control and bulk transfer only. It rejects direct device
memory instead of implying GPU-direct support, and it does not advertise
encryption. TLS remains a separate implementation requirement.

## Security

Plain TCP provides no peer authentication or encryption. Restrict it to a
trusted network until a TLS transport is available. Never expose the backend's
plain TCP listener directly to an untrusted network.
