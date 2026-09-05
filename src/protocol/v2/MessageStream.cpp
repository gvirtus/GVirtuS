#include "gvirtus/protocol/v2/MessageStream.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace gvirtus::protocol::v2 {

MessageStreamDecoder::MessageStreamDecoder(std::uint64_t maximum_payload,
                                           std::size_t maximum_buffered_bytes,
                                           std::uint16_t supported_minor,
                                           std::size_t maximum_messages_ready)
    : maximum_payload_(maximum_payload),
      maximum_buffered_bytes_(maximum_buffered_bytes),
      supported_minor_(supported_minor),
      maximum_messages_ready_(maximum_messages_ready) {
  if (maximum_payload_ >
          std::numeric_limits<std::size_t>::max() - kWireHeaderSize ||
      maximum_buffered_bytes_ < kWireHeaderSize ||
      maximum_buffered_bytes_ < maximum_payload_ + kWireHeaderSize ||
      maximum_messages_ready_ == 0)
    throw std::invalid_argument("invalid Protocol v2 stream limits");
}

StreamResult MessageStreamDecoder::Feed(const std::byte *data,
                                        std::size_t size) {
  if (error_ != StreamError::None)
    return {StreamError::FailedState, header_error_, 0, messages_.size()};
  if (data == nullptr && size != 0)
    return {StreamError::InvalidInput, ParseError::None, 0, messages_.size()};

  const auto retained = buffer_.size() - offset_;
  if (size > maximum_buffered_bytes_ - retained) {
    Fail(StreamError::BufferLimitExceeded);
    return {error_, header_error_, 0, messages_.size()};
  }
  if (offset_ != 0) {
    buffer_.erase(buffer_.begin(), buffer_.begin() +
                                      static_cast<std::ptrdiff_t>(offset_));
    offset_ = 0;
  }
  if (size != 0) buffer_.insert(buffer_.end(), data, data + size);
  ParseAvailable();
  return {error_, header_error_, size, messages_.size()};
}

bool MessageStreamDecoder::ParseAvailable() {
  while (buffer_.size() - offset_ >= kWireHeaderSize) {
    if (messages_.size() >= maximum_messages_ready_) {
      Fail(StreamError::MessageLimitExceeded);
      return false;
    }
    const auto *start = buffer_.data() + offset_;
    const auto decoded = DecodeHeader(start, kWireHeaderSize, maximum_payload_,
                                      supported_minor_);
    if (!decoded) {
      Fail(StreamError::InvalidHeader, decoded.error);
      return false;
    }
    const auto payload_size =
        static_cast<std::size_t>(decoded.header.payload_length);
    const auto frame_size = kWireHeaderSize + payload_size;
    if (buffer_.size() - offset_ < frame_size) return true;

    Message message;
    message.header = decoded.header;
    if (payload_size != 0)
      message.payload.assign(start + kWireHeaderSize, start + frame_size);
    messages_.push_back(std::move(message));
    offset_ += frame_size;
  }
  if (offset_ == buffer_.size()) {
    buffer_.clear();
    offset_ = 0;
  }
  return true;
}

bool MessageStreamDecoder::HasMessage() const { return !messages_.empty(); }

std::size_t MessageStreamDecoder::messages_ready() const {
  return messages_.size();
}

Message MessageStreamDecoder::PopMessage() {
  if (messages_.empty()) throw std::out_of_range("no Protocol v2 message ready");
  auto message = std::move(messages_.front());
  messages_.pop_front();
  return message;
}

std::size_t MessageStreamDecoder::buffered_bytes() const {
  return buffer_.size() - offset_;
}

bool MessageStreamDecoder::failed() const {
  return error_ != StreamError::None;
}

StreamError MessageStreamDecoder::error() const { return error_; }

ParseError MessageStreamDecoder::header_error() const { return header_error_; }

void MessageStreamDecoder::Reset() {
  buffer_.clear();
  offset_ = 0;
  messages_.clear();
  error_ = StreamError::None;
  header_error_ = ParseError::None;
}

void MessageStreamDecoder::Fail(StreamError error, ParseError header_error) {
  error_ = error;
  header_error_ = header_error;
  buffer_.clear();
  offset_ = 0;
}

const char *ToString(StreamError error) {
  switch (error) {
    case StreamError::None: return "none";
    case StreamError::InvalidInput: return "invalid stream input";
    case StreamError::BufferLimitExceeded: return "stream buffer limit exceeded";
    case StreamError::MessageLimitExceeded: return "ready message limit exceeded";
    case StreamError::InvalidHeader: return "invalid frame header";
    case StreamError::FailedState: return "stream decoder is failed";
  }
  return "unknown stream error";
}

}  // namespace gvirtus::protocol::v2
