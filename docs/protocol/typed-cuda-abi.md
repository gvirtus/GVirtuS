# Protocol v2 typed CUDA ABI

Protocol v2 does not serialize native CUDA or C++ structures. Its CUDA ABI uses
fixed-width fields, explicit network byte order, a version number, and zeroed
reserved fields.

The initial schema defines typed representations for three-dimensional launch
dimensions, extents and positions, pitched pointers, channel descriptors,
memory-copy endpoints, launch configuration, and kernel parameters.

Kernel parameters have an explicit semantic kind: scalar, device pointer,
object handle, or structured value. Only values identified as device pointers
by authoritative metadata may be translated through the virtual-memory
registry. Pointer-sized scalar values must never be inspected heuristically.

Memory endpoints identify bounded host payloads, virtual device pointers, or
virtual arrays. Host pointer addresses have no wire meaning.

## Validation

Decoders reject unsupported ABI versions, truncated structures, non-zero
reserved fields, zero launch dimensions, invalid endpoint kinds and ranges,
zero-sized parameters, non-power-of-two alignment, misaligned offsets, and
offset arithmetic overflow. Pitched layouts require a pitch at least as large
as the logical row width and overflow-safe total-size arithmetic.

The first encoded structures are the 56-byte launch configuration and 24-byte
kernel-parameter descriptor. Additional compound copy encodings will compose
these primitives rather than importing native `cudaMemcpy3DParms` layout.
