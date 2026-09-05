#include "gvirtus/protocol/v2/Frame.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

using namespace gvirtus::protocol::v2;

int main() {
  FrameHeader header;
  header.message_type = MessageType::Request;
  header.flags = 0x0001;
  header.session_id = 0x0102030405060708ULL;
  header.request_id = 0x1112131415161718ULL;
  header.api_namespace = 0x21222324U;
  header.operation_id = 0x31323334U;
  header.payload_length = 4096;

  const auto wire = EncodeHeader(header);
  assert(wire.size() == kWireHeaderSize);
  assert(wire[0] == std::byte{'G'} && wire[1] == std::byte{'V'} &&
         wire[2] == std::byte{'R'} && wire[3] == std::byte{'2'});
  assert(wire[12] == std::byte{0x01} && wire[19] == std::byte{0x08});

  const auto decoded = DecodeHeader(wire.data(), wire.size());
  assert(decoded);
  assert(decoded.header.message_type == MessageType::Request);
  assert(decoded.header.flags == header.flags);
  assert(decoded.header.session_id == header.session_id);
  assert(decoded.header.request_id == header.request_id);
  assert(decoded.header.api_namespace == header.api_namespace);
  assert(decoded.header.operation_id == header.operation_id);
  assert(decoded.header.payload_length == header.payload_length);

  assert(DecodeHeader(wire.data(), wire.size() - 1).error ==
         ParseError::Truncated);

  auto malformed = wire;
  malformed[0] = std::byte{0};
  assert(DecodeHeader(malformed.data(), malformed.size()).error ==
         ParseError::InvalidMagic);

  malformed = wire;
  malformed[8] = std::byte{0};
  malformed[9] = std::byte{0};
  assert(DecodeHeader(malformed.data(), malformed.size()).error ==
         ParseError::InvalidMessageType);

  malformed = wire;
  malformed[43] = std::byte{1};
  assert(DecodeHeader(malformed.data(), malformed.size(), 4096).error ==
         ParseError::PayloadTooLarge);

  malformed = wire;
  malformed[20] ^= std::byte{1};
  assert(DecodeHeader(malformed.data(), malformed.size()).error ==
         ParseError::InvalidChecksum);

  FrameHeader future = header;
  future.protocol_minor = kProtocolMinor + 1;
  const auto future_wire = EncodeHeader(future);
  assert(DecodeHeader(future_wire.data(), future_wire.size()).error ==
         ParseError::UnsupportedVersion);
  return 0;
}
