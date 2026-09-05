#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gvirtus/protocol/v2/Frame.h"

namespace gvirtus::protocol::v2 {

enum class MessageError {
  None,
  InvalidPayload,
  PayloadLengthMismatch,
  FrameTooLarge,
  TruncatedPayload,
  TrailingData,
  InvalidHeader,
};

struct Message {
  FrameHeader header;
  std::vector<std::byte> payload;
};

struct EncodeMessageResult {
  std::vector<std::byte> wire;
  MessageError error = MessageError::None;
  explicit operator bool() const { return error == MessageError::None; }
};

struct DecodeMessageResult {
  Message message;
  MessageError error = MessageError::None;
  ParseError header_error = ParseError::None;
  explicit operator bool() const { return error == MessageError::None; }
};

EncodeMessageResult EncodeMessage(
    const FrameHeader &header, const std::byte *payload,
    std::size_t payload_size,
    std::uint64_t max_payload_bytes = kDefaultMaxPayloadBytes);

DecodeMessageResult DecodeMessage(
    const std::byte *wire, std::size_t wire_size,
    std::uint64_t max_payload_bytes = kDefaultMaxPayloadBytes,
    std::uint16_t supported_minor = kProtocolMinor);

const char *ToString(MessageError error);

}  // namespace gvirtus::protocol::v2
