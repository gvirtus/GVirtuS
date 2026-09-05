#include "gvirtus/protocol/v2/VirtualObjects.h"

namespace gvirtus::protocol::v2 {
namespace {

constexpr std::uint64_t kSequenceMask = 0x000000ffffffffffULL;

}  // namespace

VirtualObjectRegistry::VirtualObjectRegistry(std::uint16_t session_namespace,
                                             std::size_t max_objects)
    : session_namespace_(session_namespace), max_objects_(max_objects) {}

ObjectResult VirtualObjectRegistry::Register(ObjectType type,
                                             BackendObject backend_object) {
  if (session_namespace_ == 0)
    return {{}, ObjectError::InvalidSessionNamespace};
  if (!IsValidType(type)) return {{}, ObjectError::InvalidObjectType};
  if (backend_object == 0) return {{}, ObjectError::InvalidBackendObject};

  std::lock_guard<std::mutex> lock(mutex_);
  if (active_.size() >= max_objects_)
    return {{}, ObjectError::CapacityExceeded};
  if (next_sequence_ == 0 || next_sequence_ > kSequenceMask)
    return {{}, ObjectError::TokenSpaceExhausted};

  ObjectRecord record;
  record.token = (static_cast<std::uint64_t>(session_namespace_) << 48U) |
                 (static_cast<std::uint64_t>(type) << 40U) |
                 next_sequence_++;
  record.backend_object = backend_object;
  record.type = type;
  active_.emplace(record.token, record);
  return {record, ObjectError::None};
}

ObjectResult VirtualObjectRegistry::Resolve(VirtualObject token,
                                            ObjectType expected_type) const {
  const auto validation = ValidateToken(token, expected_type);
  if (validation != ObjectError::None) return {{}, validation};

  std::lock_guard<std::mutex> lock(mutex_);
  const auto object = active_.find(token);
  if (object != active_.end()) return {object->second, ObjectError::None};
  if (retired_.find(token) != retired_.end())
    return {{}, ObjectError::StaleHandle};
  return {{}, ObjectError::UnknownHandle};
}

ObjectResult VirtualObjectRegistry::Release(VirtualObject token,
                                            ObjectType expected_type) {
  const auto validation = ValidateToken(token, expected_type);
  if (validation != ObjectError::None) return {{}, validation};

  std::lock_guard<std::mutex> lock(mutex_);
  const auto object = active_.find(token);
  if (object == active_.end()) {
    if (retired_.find(token) != retired_.end())
      return {{}, ObjectError::StaleHandle};
    return {{}, ObjectError::UnknownHandle};
  }
  const auto record = object->second;
  retired_.insert(token);
  active_.erase(object);
  return {record, ObjectError::None};
}

std::vector<ObjectRecord> VirtualObjectRegistry::ReleaseAll() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<ObjectRecord> released;
  released.reserve(active_.size());
  for (const auto &entry : active_) {
    released.push_back(entry.second);
    retired_.insert(entry.first);
  }
  active_.clear();
  return released;
}

std::size_t VirtualObjectRegistry::active_objects() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_.size();
}

bool VirtualObjectRegistry::IsValidType(ObjectType type) {
  const auto value = static_cast<std::uint8_t>(type);
  return value >= static_cast<std::uint8_t>(ObjectType::Stream) &&
         value <= static_cast<std::uint8_t>(ObjectType::Descriptor);
}

ObjectType VirtualObjectRegistry::TokenType(VirtualObject token) {
  return static_cast<ObjectType>((token >> 40U) & 0xffU);
}

ObjectError VirtualObjectRegistry::ValidateToken(
    VirtualObject token, ObjectType expected_type) const {
  if (session_namespace_ == 0)
    return ObjectError::InvalidSessionNamespace;
  if (!IsValidType(expected_type)) return ObjectError::InvalidObjectType;
  if (static_cast<std::uint16_t>(token >> 48U) != session_namespace_)
    return ObjectError::CrossSessionHandle;
  if (TokenType(token) != expected_type) return ObjectError::WrongObjectType;
  return ObjectError::None;
}

const char *ToString(ObjectError error) {
  switch (error) {
    case ObjectError::None:
      return "none";
    case ObjectError::InvalidSessionNamespace:
      return "session namespace must be non-zero";
    case ObjectError::InvalidBackendObject:
      return "backend object must be non-zero";
    case ObjectError::InvalidObjectType:
      return "invalid object type";
    case ObjectError::CapacityExceeded:
      return "session object limit exceeded";
    case ObjectError::TokenSpaceExhausted:
      return "virtual object token space exhausted";
    case ObjectError::CrossSessionHandle:
      return "handle belongs to another session";
    case ObjectError::WrongObjectType:
      return "handle has the wrong object type";
    case ObjectError::UnknownHandle:
      return "unknown virtual handle";
    case ObjectError::StaleHandle:
      return "virtual handle refers to a released object";
  }
  return "unknown virtual object error";
}

}  // namespace gvirtus::protocol::v2
