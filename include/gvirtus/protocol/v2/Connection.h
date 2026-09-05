#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gvirtus/protocol/v2/Frame.h"
#include "gvirtus/protocol/v2/Negotiation.h"

namespace gvirtus::protocol::v2 {

enum class ProtocolMode { LegacyV1, V2 };

enum class PreferenceError { None, InvalidValue };

struct PreferenceResult {
  ProtocolMode mode = ProtocolMode::LegacyV1;
  PreferenceError error = PreferenceError::None;

  explicit operator bool() const { return error == PreferenceError::None; }
};

PreferenceResult ParseProtocolPreference(const char *value);

ProtocolMode DetectProtocol(const std::array<std::byte, 4> &prefix);

enum class SessionError {
  None,
  InvalidSessionId,
  SessionMismatch,
  InvalidRequestId,
  RequestIdOutOfOrder,
  Closed,
};

class SessionState {
 public:
  explicit SessionState(std::uint64_t session_id);

  SessionError ValidateIncoming(const FrameHeader &header);
  std::uint64_t NextRequestId();
  void Close();

  std::uint64_t session_id() const { return session_id_; }
  std::uint64_t last_incoming_request_id() const {
    return last_incoming_request_id_;
  }
  bool closed() const { return closed_; }

 private:
  std::uint64_t session_id_;
  std::uint64_t last_incoming_request_id_ = 0;
  std::uint64_t next_outgoing_request_id_ = 1;
  bool closed_ = false;
};

const char *ToString(SessionError error);

}  // namespace gvirtus::protocol::v2
