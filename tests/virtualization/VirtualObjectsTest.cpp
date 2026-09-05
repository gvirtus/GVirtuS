#include "gvirtus/protocol/v2/VirtualObjects.h"

#include <cassert>
#include <cstdint>

using namespace gvirtus::protocol::v2;

int main() {
  VirtualObjectRegistry registry(0x1234, 2);
  const auto stream = registry.Register(ObjectType::Stream, 0xabcdef00ULL);
  const auto event = registry.Register(ObjectType::Event, 0xabcdef10ULL);
  assert(stream && event);
  assert(stream.object.token != stream.object.backend_object);
  assert(stream.object.token >> 48U == 0x1234);
  assert(registry.active_objects() == 2);
  assert(registry.Register(ObjectType::Array, 0xabcdef20ULL).error ==
         ObjectError::CapacityExceeded);

  const auto resolved =
      registry.Resolve(stream.object.token, ObjectType::Stream);
  assert(resolved);
  assert(resolved.object.backend_object == stream.object.backend_object);
  assert(registry.Resolve(stream.object.token, ObjectType::Event).error ==
         ObjectError::WrongObjectType);

  VirtualObjectRegistry other_session(0x5678);
  assert(other_session.Resolve(stream.object.token, ObjectType::Stream).error ==
         ObjectError::CrossSessionHandle);
  assert(other_session.Release(stream.object.token, ObjectType::Stream).error ==
         ObjectError::CrossSessionHandle);

  const auto released =
      registry.Release(stream.object.token, ObjectType::Stream);
  assert(released);
  assert(released.object.backend_object == stream.object.backend_object);
  assert(registry.active_objects() == 1);
  assert(registry.Resolve(stream.object.token, ObjectType::Stream).error ==
         ObjectError::StaleHandle);
  assert(registry.Release(stream.object.token, ObjectType::Stream).error ==
         ObjectError::StaleHandle);

  const auto unknown = stream.object.token + 100;
  assert(registry.Resolve(unknown, ObjectType::Stream).error ==
         ObjectError::UnknownHandle);
  assert(registry.Register(ObjectType::Graph, 0xabcdef30ULL));

  const auto remaining = registry.ReleaseAll();
  assert(remaining.size() == 2);
  assert(registry.active_objects() == 0);
  assert(registry.Resolve(event.object.token, ObjectType::Event).error ==
         ObjectError::StaleHandle);
  assert(registry.ReleaseAll().empty());

  assert(registry.Register(ObjectType::Stream, 0).error ==
         ObjectError::InvalidBackendObject);
  const auto invalid_type = static_cast<ObjectType>(255);
  assert(registry.Register(invalid_type, 1).error ==
         ObjectError::InvalidObjectType);

  VirtualObjectRegistry invalid_namespace(0);
  assert(invalid_namespace.Register(ObjectType::Stream, 1).error ==
         ObjectError::InvalidSessionNamespace);
  assert(invalid_namespace.Resolve(1, ObjectType::Stream).error ==
         ObjectError::InvalidSessionNamespace);
  return 0;
}
