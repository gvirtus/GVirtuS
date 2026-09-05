#include "gvirtus/protocol/v2/KernelMetadata.h"

#include <algorithm>
#include <set>
#include <utility>

namespace gvirtus::protocol::v2 {
namespace {

std::uint64_t Load64(const std::byte *input) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index)
    value = (value << 8U) | static_cast<std::uint64_t>(input[index]);
  return value;
}

}  // namespace

KernelMetadataRegistry::KernelMetadataRegistry(
    std::uint16_t session_namespace)
    : session_namespace_(session_namespace) {}

KernelMetadataResult KernelMetadataRegistry::Register(
    const KernelMetadata &metadata, const VirtualObjectRegistry &objects) {
  const auto error = ValidateMetadata(metadata, objects);
  if (error != KernelMetadataError::None) return {{}, error};
  std::lock_guard<std::mutex> lock(mutex_);
  if (kernels_.find(metadata.function) != kernels_.end())
    return {{}, KernelMetadataError::DuplicateKernel};
  kernels_.emplace(metadata.function, metadata);
  return {metadata, KernelMetadataError::None};
}

KernelMetadataResult KernelMetadataRegistry::Lookup(
    VirtualObject function) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = kernels_.find(function);
  if (found == kernels_.end()) return {{}, KernelMetadataError::UnknownKernel};
  return {found->second, KernelMetadataError::None};
}

KernelMetadataResult KernelMetadataRegistry::Release(VirtualObject function) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = kernels_.find(function);
  if (found == kernels_.end()) return {{}, KernelMetadataError::UnknownKernel};
  const auto metadata = found->second;
  kernels_.erase(found);
  return {metadata, KernelMetadataError::None};
}

KernelMetadataError KernelMetadataRegistry::ValidateMetadata(
    const KernelMetadata &metadata,
    const VirtualObjectRegistry &objects) const {
  if (session_namespace_ == 0)
    return KernelMetadataError::InvalidSessionNamespace;
  if (!objects.Resolve(metadata.function, ObjectType::Function))
    return KernelMetadataError::InvalidFunctionHandle;
  if (metadata.mangled_name.empty() ||
      metadata.mangled_name.size() > kMaxKernelNameBytes)
    return KernelMetadataError::InvalidName;
  if (metadata.metadata_version != 1)
    return KernelMetadataError::UnsupportedMetadataVersion;
  if (metadata.argument_size == 0)
    return KernelMetadataError::InvalidArgumentSize;
  if (metadata.parameters.size() > kMaxParameters)
    return KernelMetadataError::TooManyParameters;

  std::set<std::uint32_t> indexes;
  std::set<std::uint32_t> ordinals;
  std::vector<KernelParameterDescriptor> ordered = metadata.parameters;
  for (const auto &parameter : ordered) {
    if (Validate(parameter) != AbiError::None)
      return KernelMetadataError::InvalidParameter;
    if (!indexes.insert(parameter.index).second)
      return KernelMetadataError::DuplicateIndex;
    if (!ordinals.insert(parameter.ordinal).second)
      return KernelMetadataError::DuplicateOrdinal;
    if ((parameter.kind == KernelParameterKind::DevicePointer ||
         parameter.kind == KernelParameterKind::ObjectHandle) &&
        parameter.size != sizeof(std::uint64_t))
      return KernelMetadataError::InvalidPointerWidth;
    if (parameter.offset > metadata.argument_size ||
        parameter.size > metadata.argument_size - parameter.offset)
      return KernelMetadataError::ParameterOutOfBounds;
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto &left,
                                               const auto &right) {
    return left.offset < right.offset;
  });
  for (std::size_t index = 1; index < ordered.size(); ++index) {
    const auto previous_end = ordered[index - 1].offset +
                              ordered[index - 1].size;
    if (previous_end > ordered[index].offset)
      return KernelMetadataError::OverlappingParameters;
  }
  return KernelMetadataError::None;
}

LaunchPreparationResult PrepareKernelArguments(
    const KernelMetadata &metadata, const std::byte *argument_blob,
    std::size_t argument_blob_size, const VirtualMemoryRegistry &memory,
    const VirtualObjectRegistry &objects) {
  if (argument_blob == nullptr || argument_blob_size != metadata.argument_size)
    return {{}, KernelMetadataError::ArgumentBlobSizeMismatch};
  LaunchPreparationResult result;
  result.parameters.reserve(metadata.parameters.size());
  for (const auto &descriptor : metadata.parameters) {
    PreparedKernelParameter parameter;
    parameter.descriptor = descriptor;
    parameter.value.assign(argument_blob + descriptor.offset,
                           argument_blob + descriptor.offset + descriptor.size);
    if (descriptor.kind == KernelParameterKind::DevicePointer) {
      const auto translated = memory.Resolve(Load64(parameter.value.data()), 1);
      if (!translated)
        return {{}, KernelMetadataError::MemoryTranslationFailed};
      parameter.translated_identity = translated.backend_address;
      parameter.managed_memory =
          translated.allocation.type == AllocationType::Managed;
    } else if (descriptor.kind == KernelParameterKind::ObjectHandle) {
      const auto translated = objects.Resolve(Load64(parameter.value.data()),
                                              descriptor.object_type);
      if (!translated)
        return {{}, KernelMetadataError::ObjectTranslationFailed};
      parameter.translated_identity = translated.object.backend_object;
    }
    result.parameters.push_back(std::move(parameter));
  }
  return result;
}

const char *ToString(KernelMetadataError error) {
  switch (error) {
    case KernelMetadataError::None: return "none";
    case KernelMetadataError::InvalidSessionNamespace: return "session namespace must be non-zero";
    case KernelMetadataError::InvalidFunctionHandle: return "invalid virtual function handle";
    case KernelMetadataError::InvalidName: return "invalid kernel name";
    case KernelMetadataError::UnsupportedMetadataVersion: return "unsupported metadata version";
    case KernelMetadataError::InvalidArgumentSize: return "invalid argument blob size";
    case KernelMetadataError::TooManyParameters: return "too many kernel parameters";
    case KernelMetadataError::InvalidParameter: return "invalid kernel parameter";
    case KernelMetadataError::DuplicateIndex: return "duplicate parameter index";
    case KernelMetadataError::DuplicateOrdinal: return "duplicate parameter ordinal";
    case KernelMetadataError::OverlappingParameters: return "kernel parameters overlap";
    case KernelMetadataError::ParameterOutOfBounds: return "kernel parameter is out of bounds";
    case KernelMetadataError::InvalidPointerWidth: return "pointer or handle parameter is not 64 bits";
    case KernelMetadataError::DuplicateKernel: return "kernel metadata is already registered";
    case KernelMetadataError::UnknownKernel: return "unknown kernel metadata";
    case KernelMetadataError::ArgumentBlobSizeMismatch: return "argument blob size does not match metadata";
    case KernelMetadataError::MemoryTranslationFailed: return "device pointer translation failed";
    case KernelMetadataError::ObjectTranslationFailed: return "object handle translation failed";
  }
  return "unknown kernel metadata error";
}

}  // namespace gvirtus::protocol::v2
