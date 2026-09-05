#include "gvirtus/protocol/v2/Connection.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace gvirtus::protocol::v2 {

PreferenceResult ParseProtocolPreference(const char *value) {
  if (value == nullptr || *value == '\0')
    return {ProtocolMode::LegacyV1, PreferenceError::None};

  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  if (normalized == "v1" || normalized == "1")
    return {ProtocolMode::LegacyV1, PreferenceError::None};
  if (normalized == "v2" || normalized == "2")
    return {ProtocolMode::V2, PreferenceError::None};
  return {ProtocolMode::LegacyV1, PreferenceError::InvalidValue};
}

ProtocolMode DetectProtocol(const std::array<std::byte, 4> &prefix) {
  const std::array<std::byte, 4> v2_magic{
      std::byte{'G'}, std::byte{'V'}, std::byte{'R'}, std::byte{'2'}};
  return prefix == v2_magic ? ProtocolMode::V2 : ProtocolMode::LegacyV1;
}

SessionState::SessionState(std::uint64_t session_id) : session_id_(session_id) {}

SessionError SessionState::ValidateIncoming(const FrameHeader &header) {
  if (closed_) return SessionError::Closed;
  if (session_id_ == 0) return SessionError::InvalidSessionId;
  if (header.session_id != session_id_) return SessionError::SessionMismatch;

  if (header.message_type == MessageType::Request) {
    if (header.request_id == 0) return SessionError::InvalidRequestId;
    if (header.request_id <= last_incoming_request_id_)
      return SessionError::RequestIdOutOfOrder;
    last_incoming_request_id_ = header.request_id;
  }
  return SessionError::None;
}

std::uint64_t SessionState::NextRequestId() {
  if (closed_ || next_outgoing_request_id_ == 0)
    return 0;
  const auto request_id = next_outgoing_request_id_;
  if (next_outgoing_request_id_ == std::numeric_limits<std::uint64_t>::max())
    next_outgoing_request_id_ = 0;
  else
    ++next_outgoing_request_id_;
  return request_id;
}

void SessionState::Close() { closed_ = true; }

const char *ToString(SessionError error) {
  switch (error) {
    case SessionError::None:
      return "none";
    case SessionError::InvalidSessionId:
      return "session id must be non-zero";
    case SessionError::SessionMismatch:
      return "frame belongs to another session";
    case SessionError::InvalidRequestId:
      return "request id must be non-zero";
    case SessionError::RequestIdOutOfOrder:
      return "request id is stale or duplicated";
    case SessionError::Closed:
      return "session is closed";
  }
  return "unknown session error";
}

}  // namespace gvirtus::protocol::v2
