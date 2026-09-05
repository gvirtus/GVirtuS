#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace gvirtus::protocol::v2 {

using VirtualObject = std::uint64_t;
using BackendObject = std::uint64_t;

enum class ObjectType : std::uint8_t {
  Stream = 1,
  Event = 2,
  Array = 3,
  MipmappedArray = 4,
  Graph = 5,
  GraphExecution = 6,
  Module = 7,
  Function = 8,
  Texture = 9,
  Surface = 10,
  LibraryHandle = 11,
  Descriptor = 12,
};

enum class ObjectError {
  None,
  InvalidSessionNamespace,
  InvalidBackendObject,
  InvalidObjectType,
  CapacityExceeded,
  TokenSpaceExhausted,
  CrossSessionHandle,
  WrongObjectType,
  UnknownHandle,
  StaleHandle,
};

struct ObjectRecord {
  VirtualObject token = 0;
  BackendObject backend_object = 0;
  ObjectType type = ObjectType::Stream;
};

struct ObjectResult {
  ObjectRecord object{};
  ObjectError error = ObjectError::None;

  explicit operator bool() const { return error == ObjectError::None; }
};

class VirtualObjectRegistry {
 public:
  static constexpr std::size_t kDefaultMaxObjects = 65536;

  explicit VirtualObjectRegistry(
      std::uint16_t session_namespace,
      std::size_t max_objects = kDefaultMaxObjects);

  ObjectResult Register(ObjectType type, BackendObject backend_object);
  ObjectResult Resolve(VirtualObject token, ObjectType expected_type) const;
  ObjectResult Release(VirtualObject token, ObjectType expected_type);
  std::vector<ObjectRecord> ReleaseAll();

  std::size_t active_objects() const;
  std::uint16_t session_namespace() const { return session_namespace_; }

 private:
  static bool IsValidType(ObjectType type);
  static ObjectType TokenType(VirtualObject token);
  ObjectError ValidateToken(VirtualObject token,
                            ObjectType expected_type) const;

  const std::uint16_t session_namespace_;
  const std::size_t max_objects_;
  std::uint64_t next_sequence_ = 1;
  mutable std::mutex mutex_;
  std::map<VirtualObject, ObjectRecord> active_;
  std::set<VirtualObject> retired_;
};

const char *ToString(ObjectError error);

}  // namespace gvirtus::protocol::v2
