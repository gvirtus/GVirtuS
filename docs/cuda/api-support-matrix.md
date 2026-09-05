# CUDA API support matrix

This matrix distinguishes legacy implementation, Protocol v2 modeling, and
actual CUDA integration. “Typed model” does not mean that a CUDA-capable
end-to-end test has passed.

| API or subsystem | Protocol v1 | Protocol v2 | Virtualized | CPU tested | GPU tested |
| --- | --- | --- | --- | --- | --- |
| `cudaMalloc` / `cudaFree` | Legacy | Registry foundation | Yes | Yes | No |
| Streams and events | Legacy | Object registry | Yes | Yes | No |
| CUDA arrays | Partial legacy | Object registry | Yes | Yes | No |
| `cudaMalloc3D` | Stub | Typed request | Planned | Yes | No |
| `cudaMemcpy2D` | Legacy | Typed request | Endpoint model | Yes | No |
| `cudaMemcpy2DAsync` | Stub | Typed request | Endpoint model | Yes | No |
| `cudaMemcpy3D` | Native-layout legacy | Typed request | Endpoint model | Yes | No |
| `cudaMemcpy3DAsync` | Stub | Typed request | Endpoint model | Yes | No |
| `cudaMallocManaged` | Historical/legacy | Coherence state machine | Registry model | Yes | No |
| Kernel parameter metadata | Historical registration | Typed descriptor | Planned | Yes | No |
| `cudaLaunchKernel` | Legacy | Typed launch configuration | Planned | Yes | No |

Protocol v1 behavior remains available and is not routed through these v2
structures. GPU-tested status requires a CUDA-capable frontend/backend CI run;
none has been performed for the Protocol v2 entries above.
