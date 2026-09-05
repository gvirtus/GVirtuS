#include "gvirtus/protocol/v2/Negotiation.h"

#include <cassert>
#include <cstddef>

using namespace gvirtus::protocol::v2;

int main() {
  Hello hello;
  hello.capabilities =
      Capability::AsyncRequests | Capability::BulkTransfer;
  hello.maximum_payload_bytes = 1024 * 1024;
  hello.maximum_outstanding_requests = 128;

  const auto wire = EncodeHello(hello);
  const auto decoded = DecodeHello(wire.data(), wire.size());
  assert(decoded);
  assert(decoded.hello.maximum_payload_bytes == hello.maximum_payload_bytes);
  assert(decoded.hello.maximum_outstanding_requests == 128);
  assert(static_cast<std::uint64_t>(decoded.hello.capabilities) ==
         static_cast<std::uint64_t>(hello.capabilities));

  assert(DecodeHello(wire.data(), wire.size() - 1).error ==
         NegotiationError::Truncated);
  assert(DecodeHello(wire.data(), wire.size(), 1024).error ==
         NegotiationError::InvalidPayloadLimit);
  assert(DecodeHello(wire.data(), wire.size(), kDefaultMaxPayloadBytes, 64)
             .error == NegotiationError::InvalidOutstandingRequestLimit);

  auto malformed = wire;
  malformed[6] = std::byte{1};
  assert(DecodeHello(malformed.data(), malformed.size()).error ==
         NegotiationError::InvalidReservedField);

  malformed = wire;
  malformed[23] = std::byte{0x80};
  assert(DecodeHello(malformed.data(), malformed.size()).error ==
         NegotiationError::UnknownCapabilities);
  return 0;
}
