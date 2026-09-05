#include "gvirtus/protocol/v2/Frame.h"

#include <algorithm>

namespace gvirtus::protocol::v2 {
namespace {

constexpr std::size_t kChecksumOffset = 44;

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

std::uint32_t Crc32(const std::byte *data, std::size_t size) {
  std::uint32_t crc = 0xffffffffU;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= static_cast<std::uint8_t>(data[i]);
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

bool IsValidMessageType(std::uint16_t value) {
  return value >= static_cast<std::uint16_t>(MessageType::Hello) &&
         value <= static_cast<std::uint16_t>(MessageType::Close);
}

}  // namespace

std::array<std::byte, kWireHeaderSize> EncodeHeader(const FrameHeader &header) {
  std::array<std::byte, kWireHeaderSize> wire{};
  Store32(wire.data(), kMagic);
  Store16(wire.data() + 4, header.protocol_major);
  Store16(wire.data() + 6, header.protocol_minor);
  Store16(wire.data() + 8, static_cast<std::uint16_t>(header.message_type));
  Store16(wire.data() + 10, header.flags);
  Store64(wire.data() + 12, header.session_id);
  Store64(wire.data() + 20, header.request_id);
  Store32(wire.data() + 28, header.api_namespace);
  Store32(wire.data() + 32, header.operation_id);
  Store64(wire.data() + 36, header.payload_length);
  Store32(wire.data() + kChecksumOffset, Crc32(wire.data(), kChecksumOffset));
  return wire;
}

DecodeResult DecodeHeader(const std::byte *wire, std::size_t wire_size,
                          std::uint64_t max_payload_bytes,
                          std::uint16_t supported_minor) {
  if (wire == nullptr || wire_size < kWireHeaderSize)
    return {{}, ParseError::Truncated};
  if (Load32(wire) != kMagic) return {{}, ParseError::InvalidMagic};

  FrameHeader header;
  header.protocol_major = Load16(wire + 4);
  header.protocol_minor = Load16(wire + 6);
  if (header.protocol_major != kProtocolMajor ||
      header.protocol_minor > supported_minor)
    return {{}, ParseError::UnsupportedVersion};

  const auto message_type = Load16(wire + 8);
  if (!IsValidMessageType(message_type))
    return {{}, ParseError::InvalidMessageType};
  header.message_type = static_cast<MessageType>(message_type);
  header.flags = Load16(wire + 10);
  header.session_id = Load64(wire + 12);
  header.request_id = Load64(wire + 20);
  header.api_namespace = Load32(wire + 28);
  header.operation_id = Load32(wire + 32);
  header.payload_length = Load64(wire + 36);
  if (header.payload_length > max_payload_bytes)
    return {{}, ParseError::PayloadTooLarge};

  const auto expected_checksum = Load32(wire + kChecksumOffset);
  if (expected_checksum != Crc32(wire, kChecksumOffset))
    return {{}, ParseError::InvalidChecksum};
  return {header, ParseError::None};
}

const char *ToString(ParseError error) {
  switch (error) {
    case ParseError::None:
      return "none";
    case ParseError::Truncated:
      return "truncated header";
    case ParseError::InvalidMagic:
      return "invalid magic";
    case ParseError::UnsupportedVersion:
      return "unsupported protocol version";
    case ParseError::InvalidMessageType:
      return "invalid message type";
    case ParseError::PayloadTooLarge:
      return "payload exceeds configured limit";
    case ParseError::InvalidChecksum:
      return "invalid header checksum";
  }
  return "unknown parse error";
}

}  // namespace gvirtus::protocol::v2
