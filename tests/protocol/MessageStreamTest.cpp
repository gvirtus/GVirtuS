#include "gvirtus/protocol/v2/MessageStream.h"

#include <cassert>

using namespace gvirtus::protocol::v2;

namespace {
std::vector<std::byte> Wire(std::uint64_t request_id,
                            const std::vector<std::byte> &payload) {
  FrameHeader header;
  header.message_type = MessageType::Request;
  header.session_id = 4;
  header.request_id = request_id;
  header.payload_length = payload.size();
  const auto encoded = EncodeMessage(header, payload.data(), payload.size());
  assert(encoded);
  return encoded.wire;
}
}  // namespace

int main() {
  const auto first = Wire(1, {std::byte{'a'}, std::byte{'b'}});
  const auto second = Wire(2, {std::byte{'c'}});
  std::vector<std::byte> combined = first;
  combined.insert(combined.end(), second.begin(), second.end());

  MessageStreamDecoder decoder(16, 128);
  assert(decoder.Feed(combined.data(), 3));
  assert(!decoder.HasMessage() && decoder.buffered_bytes() == 3);
  assert(decoder.Feed(combined.data() + 3, kWireHeaderSize - 3));
  assert(!decoder.HasMessage());
  assert(decoder.Feed(combined.data() + kWireHeaderSize, 1));
  assert(!decoder.HasMessage());
  assert(decoder.Feed(combined.data() + kWireHeaderSize + 1,
                      combined.size() - kWireHeaderSize - 1));
  assert(decoder.messages_ready() == 2);
  assert(decoder.buffered_bytes() == 0);
  const auto decoded_first = decoder.PopMessage();
  assert(decoded_first.header.request_id == 1);
  assert(decoded_first.payload.size() == 2);
  assert(decoder.PopMessage().header.request_id == 2);

  assert(decoder.Feed(nullptr, 0));
  assert(decoder.Feed(nullptr, 1).error == StreamError::InvalidInput);
  assert(!decoder.failed());

  auto corrupt = first;
  corrupt[0] = std::byte{0};
  const auto malformed = decoder.Feed(corrupt.data(), corrupt.size());
  assert(malformed.error == StreamError::InvalidHeader);
  assert(malformed.header_error == ParseError::InvalidMagic);
  assert(decoder.failed());
  assert(decoder.Feed(first.data(), first.size()).error ==
         StreamError::FailedState);
  decoder.Reset();
  assert(decoder.Feed(first.data(), first.size()));
  assert(decoder.HasMessage());

  MessageStreamDecoder bounded(4, kWireHeaderSize + 4);
  std::vector<std::byte> excess(kWireHeaderSize + 5);
  assert(bounded.Feed(excess.data(), excess.size()).error ==
         StreamError::BufferLimitExceeded);

  const auto empty = Wire(3, {});
  std::vector<std::byte> two_empty = empty;
  two_empty.insert(two_empty.end(), empty.begin(), empty.end());
  MessageStreamDecoder queue_bounded(4, two_empty.size(), kProtocolMinor, 1);
  assert(queue_bounded.Feed(two_empty.data(), two_empty.size()).error ==
         StreamError::MessageLimitExceeded);
  return 0;
}
