#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "gvirtus/protocol/v2/Frame.h"

namespace gvirtus::protocol::v2 {

constexpr std::size_t kWireHelloSize = 32;
constexpr std::uint32_t kDefaultMaxOutstandingRequests = 1024;

enum class AuthenticationMode : std::uint16_t {
  None = 0,
  MutualTls = 1,
};

enum class Capability : std::uint64_t {
  None = 0,
  AsyncRequests = 1ULL << 0U,
  BulkTransfer = 1ULL << 1U,
  RegisteredMemory = 1ULL << 2U,
  GpuDirect = 1ULL << 3U,
};

constexpr Capability operator|(Capability left, Capability right) {
  return static_cast<Capability>(static_cast<std::uint64_t>(left) |
                                 static_cast<std::uint64_t>(right));
}

enum class NegotiationError {
  None,
  Truncated,
  InvalidMinorRange,
  InvalidAuthenticationMode,
  InvalidReservedField,
  InvalidPayloadLimit,
  InvalidOutstandingRequestLimit,
  UnknownCapabilities,
};

struct Hello {
  std::uint16_t minimum_minor = 0;
  std::uint16_t maximum_minor = kProtocolMinor;
  AuthenticationMode authentication = AuthenticationMode::None;
  std::uint64_t maximum_payload_bytes = kDefaultMaxPayloadBytes;
  Capability capabilities = Capability::None;
  std::uint32_t maximum_outstanding_requests =
      kDefaultMaxOutstandingRequests;
};

struct HelloDecodeResult {
  Hello hello{};
  NegotiationError error = NegotiationError::None;

  explicit operator bool() const { return error == NegotiationError::None; }
};

std::array<std::byte, kWireHelloSize> EncodeHello(const Hello &hello);

HelloDecodeResult DecodeHello(
    const std::byte *wire, std::size_t wire_size,
    std::uint64_t maximum_allowed_payload = kDefaultMaxPayloadBytes,
    std::uint32_t maximum_allowed_outstanding =
        kDefaultMaxOutstandingRequests);

const char *ToString(NegotiationError error);

}  // namespace gvirtus::protocol::v2
