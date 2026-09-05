# Typed CUDART memory operations

Protocol v2 assigns stable numeric operation identifiers to `cudaMalloc3D`,
two- and three-dimensional copies, and their asynchronous variants. These
requests do not contain `cudaPitchedPtr`, `cudaExtent`, `cudaPos`,
`cudaMemcpy3DParms`, host addresses, CUDA arrays, or streams in native layout.

Memory endpoints are fixed 32-byte values identifying a bounded host payload,
virtual device pointer, or virtual array. Device-pointer endpoints are resolved
through the session memory registry with overflow and range checks. Array
endpoints are resolved through the typed object registry. A host endpoint's
identity names protocol-owned payload data; it is not a frontend address.

Copy validation requires a non-zero, overflow-safe extent and verifies that
both endpoint bounds cover the requested byte volume. Explicit copy directions
must agree with the endpoint kinds. `Default` retains CUDA's direction-selection
intent without guessing pointer semantics: endpoint kinds are still explicit.

Non-zero 3D source or destination positions currently fail closed. Supporting
them safely requires explicit row and slice pitch metadata; interpreting them as
linear byte offsets would be incorrect.

The portable request and translation layer is CPU-tested. Invocation of real
`cudaMalloc3D`, `cudaMemcpy2DAsync`, `cudaMemcpy3D`, and
`cudaMemcpy3DAsync` remains pending integration with a CUDA-capable Protocol v2
backend and must not be described as GPU-tested.
