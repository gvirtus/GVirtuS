# Protocol v2 framing

Protocol v2 is introduced alongside the existing Protocol v1 implementation.
This foundation does not change connection routing or the default protocol.

## Wire header

Every v2 frame starts with a 48-byte header. Multibyte integers use network
byte order and no native C or CUDA structure layout appears on the wire.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic (`GVR2`) |
| 4 | 2 | Protocol major |
| 6 | 2 | Protocol minor |
| 8 | 2 | Message type |
| 10 | 2 | Flags |
| 12 | 8 | Session ID |
| 20 | 8 | Request ID |
| 28 | 4 | API namespace |
| 32 | 4 | Operation ID |
| 36 | 8 | Payload length |
| 44 | 4 | Header CRC-32 |

The CRC covers bytes 0 through 43. It detects accidental header corruption; it
is not authentication and must not be treated as a substitute for TLS or a
message authentication code.

Defined message types are `Hello`, `HelloAck`, `Request`, `Response`, `Error`,
`Ping`, `Pong`, and `Close`.

The fixed-width 32-byte `Hello` payload advertises the supported minor-version
range, authentication mode, maximum payload, transport capabilities, and
maximum number of outstanding requests. Decoding rejects non-zero reserved
fields, unknown capability bits, inverted version ranges, and zero or excessive
limits.

## Parser invariants

The parser rejects truncated headers, invalid magic, unsupported major or minor
versions, unknown message types, payload lengths above the caller's negotiated
limit, and invalid checksums before allocating or reading a payload. The
default payload ceiling is 64 MiB.

Negotiation policy, connection auto-detection, and Protocol v1/v2 routing are
intentionally deferred to the Protocol v2 integration increment.
