#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gvirtus::protocol::v2 {

constexpr std::uint32_t kMagic = 0x47565232U;  // "GVR2"
constexpr std::uint16_t kProtocolMajor = 2;
constexpr std::uint16_t kProtocolMinor = 0;
constexpr std::size_t kWireHeaderSize = 48;
constexpr std::uint64_t kDefaultMaxPayloadBytes = 64ULL * 1024ULL * 1024ULL;

enum class MessageType : std::uint16_t {
  Hello = 1,
  HelloAck = 2,
  Request = 3,
  Response = 4,
  Error = 5,
  Ping = 6,
  Pong = 7,
  Close = 8,
};

enum class ParseError {
  None,
  Truncated,
  InvalidMagic,
  UnsupportedVersion,
  InvalidMessageType,
  PayloadTooLarge,
  InvalidChecksum,
};

struct FrameHeader {
  MessageType message_type = MessageType::Hello;
  std::uint16_t flags = 0;
  std::uint64_t session_id = 0;
  std::uint64_t request_id = 0;
  std::uint32_t api_namespace = 0;
  std::uint32_t operation_id = 0;
  std::uint64_t payload_length = 0;
  std::uint16_t protocol_major = kProtocolMajor;
  std::uint16_t protocol_minor = kProtocolMinor;
};

struct DecodeResult {
  FrameHeader header{};
  ParseError error = ParseError::None;

  explicit operator bool() const { return error == ParseError::None; }
};

std::array<std::byte, kWireHeaderSize> EncodeHeader(const FrameHeader &header);

DecodeResult DecodeHeader(
    const std::byte *wire, std::size_t wire_size,
    std::uint64_t max_payload_bytes = kDefaultMaxPayloadBytes,
    std::uint16_t supported_minor = kProtocolMinor);

const char *ToString(ParseError error);

}  // namespace gvirtus::protocol::v2
