# Protocol v2 virtual device memory

Protocol v2 never exposes a backend CUDA address to a frontend. Each connection
owns a virtual-memory registry identified by a non-zero 16-bit namespace. The
namespace occupies the high bits of an opaque 64-bit token; the remaining bits
form a monotonically allocated virtual range. These tokens contain no backend
address information.

An allocation record contains its virtual base, backend base, byte length,
device ordinal, and allocation type. Backend addresses exist only in backend
session state and are not serialized into Protocol v2 responses.

## Translation

Pointer arithmetic is supported by locating the allocation containing
`virtual_address`, computing its offset, checking `offset + length` without
overflow, and applying the validated offset to the backend base. Translation
rejects:

- pointers from another session namespace;
- unknown pointers;
- pointers into released allocations;
- ranges extending beyond an allocation;
- backend-address arithmetic overflow.

Only an allocation's exact virtual base may be released. Freeing an interior
pointer is rejected. Released ranges are retained as tombstones so later uses
produce a stale-pointer error instead of an ambiguous unknown-pointer error.

The registry bounds the number of active allocations per session and returns an
explicit error when its virtual range is exhausted. The initial implementation
uses a 256-byte virtual-range alignment; this is token allocation policy, not a
claim about CUDA allocation alignment.
