#include "gvirtus/protocol/v2/Message.h"

#include <array>
#include <cassert>

using namespace gvirtus::protocol::v2;

int main() {
  const std::array<std::byte, 5> payload{
      std::byte{'h'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'},
      std::byte{'o'}};
  FrameHeader header;
  header.message_type = MessageType::Request;
  header.session_id = 9;
  header.request_id = 17;
  header.api_namespace = 2;
  header.operation_id = 3;
  header.payload_length = payload.size();

  const auto encoded =
      EncodeMessage(header, payload.data(), payload.size(), payload.size());
  assert(encoded);
  assert(encoded.wire.size() == kWireHeaderSize + payload.size());
  const auto decoded =
      DecodeMessage(encoded.wire.data(), encoded.wire.size(), payload.size());
  assert(decoded);
  assert(decoded.message.header.request_id == 17);
  assert(decoded.message.payload ==
         std::vector<std::byte>(payload.begin(), payload.end()));

  assert(DecodeMessage(encoded.wire.data(), encoded.wire.size() - 1,
                       payload.size()).error ==
         MessageError::TruncatedPayload);
  auto trailing = encoded.wire;
  trailing.push_back(std::byte{0});
  assert(DecodeMessage(trailing.data(), trailing.size(), payload.size()).error ==
         MessageError::TrailingData);

  FrameHeader mismatched = header;
  mismatched.payload_length = payload.size() - 1;
  assert(EncodeMessage(mismatched, payload.data(), payload.size()).error ==
         MessageError::PayloadLengthMismatch);
  assert(EncodeMessage(header, payload.data(), payload.size(), payload.size() - 1)
             .error == MessageError::FrameTooLarge);
  assert(EncodeMessage(header, nullptr, payload.size()).error ==
         MessageError::InvalidPayload);

  auto corrupt = encoded.wire;
  corrupt[0] = std::byte{0};
  const auto invalid =
      DecodeMessage(corrupt.data(), corrupt.size(), payload.size());
  assert(invalid.error == MessageError::InvalidHeader);
  assert(invalid.header_error == ParseError::InvalidMagic);

  FrameHeader empty;
  empty.message_type = MessageType::Ping;
  const auto encoded_empty = EncodeMessage(empty, nullptr, 0);
  assert(encoded_empty);
  assert(DecodeMessage(encoded_empty.wire.data(), encoded_empty.wire.size()));
  return 0;
}
