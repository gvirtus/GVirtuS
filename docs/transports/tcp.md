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

## Security

Plain TCP provides no peer authentication or encryption. Restrict it to a
trusted network until a TLS transport is available. Never expose the backend's
plain TCP listener directly to an untrusted network.
