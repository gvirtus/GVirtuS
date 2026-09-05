#include "gvirtus/protocol/v2/Negotiation.h"

namespace gvirtus::protocol::v2 {
namespace {

constexpr std::uint64_t kKnownCapabilities =
    static_cast<std::uint64_t>(Capability::AsyncRequests) |
    static_cast<std::uint64_t>(Capability::BulkTransfer) |
    static_cast<std::uint64_t>(Capability::RegisteredMemory) |
    static_cast<std::uint64_t>(Capability::GpuDirect);

void Store16(std::byte *output, std::uint16_t value) {
  output[0] = static_cast<std::byte>((value >> 8U) & 0xffU);
  output[1] = static_cast<std::byte>(value & 0xffU);
}

void Store32(std::byte *output, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i)
    output[i] = static_cast<std::byte>((value >> (24U - 8U * i)) & 0xffU);
}

void Store64(std::byte *output, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    output[i] = static_cast<std::byte>((value >> (56U - 8U * i)) & 0xffU);
}

std::uint16_t Load16(const std::byte *input) {
  return (static_cast<std::uint16_t>(input[0]) << 8U) |
         static_cast<std::uint16_t>(input[1]);
}

std::uint32_t Load32(const std::byte *input) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < 4; ++i)
    value = (value << 8U) | static_cast<std::uint32_t>(input[i]);
  return value;
}

std::uint64_t Load64(const std::byte *input) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8; ++i)
    value = (value << 8U) | static_cast<std::uint64_t>(input[i]);
  return value;
}

}  // namespace

std::array<std::byte, kWireHelloSize> EncodeHello(const Hello &hello) {
  std::array<std::byte, kWireHelloSize> wire{};
  Store16(wire.data(), hello.minimum_minor);
  Store16(wire.data() + 2, hello.maximum_minor);
  Store16(wire.data() + 4,
          static_cast<std::uint16_t>(hello.authentication));
  Store64(wire.data() + 8, hello.maximum_payload_bytes);
  Store64(wire.data() + 16,
          static_cast<std::uint64_t>(hello.capabilities));
  Store32(wire.data() + 24, hello.maximum_outstanding_requests);
  return wire;
}

HelloDecodeResult DecodeHello(const std::byte *wire, std::size_t wire_size,
                              std::uint64_t maximum_allowed_payload,
                              std::uint32_t maximum_allowed_outstanding) {
  if (wire == nullptr || wire_size < kWireHelloSize)
    return {{}, NegotiationError::Truncated};

  Hello hello;
  hello.minimum_minor = Load16(wire);
  hello.maximum_minor = Load16(wire + 2);
  if (hello.minimum_minor > hello.maximum_minor)
    return {{}, NegotiationError::InvalidMinorRange};

  const auto authentication = Load16(wire + 4);
  if (authentication >
      static_cast<std::uint16_t>(AuthenticationMode::MutualTls))
    return {{}, NegotiationError::InvalidAuthenticationMode};
  hello.authentication = static_cast<AuthenticationMode>(authentication);

  if (Load16(wire + 6) != 0 || Load32(wire + 28) != 0)
    return {{}, NegotiationError::InvalidReservedField};

  hello.maximum_payload_bytes = Load64(wire + 8);
  if (hello.maximum_payload_bytes == 0 ||
      hello.maximum_payload_bytes > maximum_allowed_payload)
    return {{}, NegotiationError::InvalidPayloadLimit};

  const auto capabilities = Load64(wire + 16);
  if ((capabilities & ~kKnownCapabilities) != 0)
    return {{}, NegotiationError::UnknownCapabilities};
  hello.capabilities = static_cast<Capability>(capabilities);

  hello.maximum_outstanding_requests = Load32(wire + 24);
  if (hello.maximum_outstanding_requests == 0 ||
      hello.maximum_outstanding_requests > maximum_allowed_outstanding)
    return {{}, NegotiationError::InvalidOutstandingRequestLimit};

  return {hello, NegotiationError::None};
}

const char *ToString(NegotiationError error) {
  switch (error) {
    case NegotiationError::None:
      return "none";
    case NegotiationError::Truncated:
      return "truncated hello payload";
    case NegotiationError::InvalidMinorRange:
      return "invalid protocol minor range";
    case NegotiationError::InvalidAuthenticationMode:
      return "invalid authentication mode";
    case NegotiationError::InvalidReservedField:
      return "reserved field is not zero";
    case NegotiationError::InvalidPayloadLimit:
      return "invalid maximum payload";
    case NegotiationError::InvalidOutstandingRequestLimit:
      return "invalid outstanding request limit";
    case NegotiationError::UnknownCapabilities:
      return "unknown capability bits";
  }
  return "unknown negotiation error";
}

}  // namespace gvirtus::protocol::v2
