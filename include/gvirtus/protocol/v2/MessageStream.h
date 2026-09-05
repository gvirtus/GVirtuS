#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "gvirtus/protocol/v2/Message.h"

namespace gvirtus::protocol::v2 {

enum class StreamError {
  None,
  InvalidInput,
  BufferLimitExceeded,
  MessageLimitExceeded,
  InvalidHeader,
  FailedState,
};

struct StreamResult {
  StreamError error = StreamError::None;
  ParseError header_error = ParseError::None;
  std::size_t accepted = 0;
  std::size_t messages_ready = 0;
  explicit operator bool() const { return error == StreamError::None; }
};

class MessageStreamDecoder {
 public:
  explicit MessageStreamDecoder(
      std::uint64_t maximum_payload = kDefaultMaxPayloadBytes,
      std::size_t maximum_buffered_bytes =
          static_cast<std::size_t>(kDefaultMaxPayloadBytes) + kWireHeaderSize,
      std::uint16_t supported_minor = kProtocolMinor,
      std::size_t maximum_messages_ready = 256);

  StreamResult Feed(const std::byte *data, std::size_t size);
  bool HasMessage() const;
  std::size_t messages_ready() const;
  Message PopMessage();
  std::size_t buffered_bytes() const;
  bool failed() const;
  StreamError error() const;
  ParseError header_error() const;
  void Reset();

 private:
  bool ParseAvailable();
  void Fail(StreamError error, ParseError header_error = ParseError::None);

  std::uint64_t maximum_payload_;
  std::size_t maximum_buffered_bytes_;
  std::uint16_t supported_minor_;
  std::size_t maximum_messages_ready_;
  std::vector<std::byte> buffer_;
  std::size_t offset_ = 0;
  std::deque<Message> messages_;
  StreamError error_ = StreamError::None;
  ParseError header_error_ = ParseError::None;
};

const char *ToString(StreamError error);

}  // namespace gvirtus::protocol::v2
