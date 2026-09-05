# Kernel metadata and launch preparation

Protocol v2 associates each virtual function handle with authoritative kernel
metadata obtained from fatbinary/function registration, module metadata, or a
generated schema. The registry stores the mangled name, argument-blob size,
metadata version, source, and parameter descriptors.

Each parameter descriptor includes index, ordinal, offset, size, alignment,
semantic kind, and an explicit object subtype when applicable. Registration
rejects duplicate indexes or ordinals, overlapping or out-of-bounds layouts,
invalid alignment, unsupported versions, and pointer or handle parameters that
are not exactly 64 bits on the wire.

Launch preparation copies each parameter with byte operations. It translates
only parameters classified as device pointers or object handles. Scalar and
structured bytes are preserved and are never searched for values that resemble
virtual addresses. Device-pointer translation also reports whether the target
allocation is managed so the coherence service can schedule required transfers.

Virtual pointer and handle values inside the argument blob use network byte
order. The prepared backend identity is returned separately; backend-native
addresses are never written into a Protocol v2 payload.

Scalar type conversion across heterogeneous endianness requires richer
compiler-generated scalar type metadata and is not claimed by this initial
model. CUDA launch invocation and fatbinary interception still require a
CUDA-capable integration test.
