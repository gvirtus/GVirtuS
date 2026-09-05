#include "gvirtus/protocol/v2/Message.h"

#include <algorithm>
#include <limits>

namespace gvirtus::protocol::v2 {

EncodeMessageResult EncodeMessage(const FrameHeader &header,
                                  const std::byte *payload,
                                  std::size_t payload_size,
                                  std::uint64_t max_payload_bytes) {
  if (payload == nullptr && payload_size != 0)
    return {{}, MessageError::InvalidPayload};
  if (header.payload_length != payload_size)
    return {{}, MessageError::PayloadLengthMismatch};
  if (header.payload_length > max_payload_bytes ||
      header.payload_length >
          std::numeric_limits<std::size_t>::max() - kWireHeaderSize)
    return {{}, MessageError::FrameTooLarge};

  EncodeMessageResult result;
  result.wire.resize(kWireHeaderSize + payload_size);
  const auto wire_header = EncodeHeader(header);
  std::copy(wire_header.begin(), wire_header.end(), result.wire.begin());
  if (payload_size != 0)
    std::copy(payload, payload + payload_size,
              result.wire.begin() + kWireHeaderSize);
  return result;
}

DecodeMessageResult DecodeMessage(const std::byte *wire, std::size_t wire_size,
                                  std::uint64_t max_payload_bytes,
                                  std::uint16_t supported_minor) {
  const auto decoded_header =
      DecodeHeader(wire, wire_size, max_payload_bytes, supported_minor);
  if (!decoded_header)
    return {{}, MessageError::InvalidHeader, decoded_header.error};

  const auto payload_size = decoded_header.header.payload_length;
  if (payload_size >
      std::numeric_limits<std::size_t>::max() - kWireHeaderSize)
    return {{}, MessageError::FrameTooLarge, ParseError::None};
  const auto expected_size =
      kWireHeaderSize + static_cast<std::size_t>(payload_size);
  if (wire_size < expected_size)
    return {{}, MessageError::TruncatedPayload, ParseError::None};
  if (wire_size > expected_size)
    return {{}, MessageError::TrailingData, ParseError::None};

  DecodeMessageResult result;
  result.message.header = decoded_header.header;
  if (payload_size != 0)
    result.message.payload.assign(wire + kWireHeaderSize,
                                  wire + expected_size);
  return result;
}

const char *ToString(MessageError error) {
  switch (error) {
    case MessageError::None: return "none";
    case MessageError::InvalidPayload: return "invalid payload pointer";
    case MessageError::PayloadLengthMismatch: return "payload length mismatch";
    case MessageError::FrameTooLarge: return "frame exceeds configured limit";
    case MessageError::TruncatedPayload: return "truncated payload";
    case MessageError::TrailingData: return "trailing data after frame";
    case MessageError::InvalidHeader: return "invalid frame header";
  }
  return "unknown message error";
}

}  // namespace gvirtus::protocol::v2
