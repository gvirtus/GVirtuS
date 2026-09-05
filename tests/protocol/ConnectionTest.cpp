#include "gvirtus/protocol/v2/Connection.h"

#include <array>
#include <cassert>
#include <cstddef>

using namespace gvirtus::protocol::v2;

int main() {
  assert(ParseProtocolPreference(nullptr).mode == ProtocolMode::LegacyV1);
  assert(ParseProtocolPreference("").mode == ProtocolMode::LegacyV1);
  assert(ParseProtocolPreference("v1").mode == ProtocolMode::LegacyV1);
  assert(ParseProtocolPreference("V2").mode == ProtocolMode::V2);
  assert(!ParseProtocolPreference("auto"));

  const std::array<std::byte, 4> v2_prefix{
      std::byte{'G'}, std::byte{'V'}, std::byte{'R'}, std::byte{'2'}};
  const std::array<std::byte, 4> v1_prefix{
      std::byte{'c'}, std::byte{'u'}, std::byte{'d'}, std::byte{'a'}};
  assert(DetectProtocol(v2_prefix) == ProtocolMode::V2);
  assert(DetectProtocol(v1_prefix) == ProtocolMode::LegacyV1);

  SessionState invalid_session(0);
  FrameHeader request;
  request.message_type = MessageType::Request;
  request.session_id = 7;
  request.request_id = 1;
  assert(invalid_session.ValidateIncoming(request) ==
         SessionError::InvalidSessionId);

  SessionState session(7);
  assert(session.NextRequestId() == 1);
  assert(session.NextRequestId() == 2);
  assert(session.ValidateIncoming(request) == SessionError::None);
  assert(session.last_incoming_request_id() == 1);
  assert(session.ValidateIncoming(request) ==
         SessionError::RequestIdOutOfOrder);

  request.request_id = 2;
  request.session_id = 8;
  assert(session.ValidateIncoming(request) == SessionError::SessionMismatch);
  request.session_id = 7;
  assert(session.ValidateIncoming(request) == SessionError::None);

  request.request_id = 0;
  assert(session.ValidateIncoming(request) == SessionError::InvalidRequestId);
  session.Close();
  assert(session.closed());
  assert(session.NextRequestId() == 0);
  assert(session.ValidateIncoming(request) == SessionError::Closed);
  return 0;
}
