#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "gvirtus/protocol/v2/CudaAbi.h"

namespace gvirtus::protocol::v2 {

enum class MetadataSource : std::uint8_t {
  FatbinaryRegistration = 1,
  ModuleMetadata = 2,
  GeneratedSchema = 3,
};

enum class KernelMetadataError {
  None,
  InvalidSessionNamespace,
  InvalidFunctionHandle,
  InvalidName,
  UnsupportedMetadataVersion,
  InvalidArgumentSize,
  TooManyParameters,
  InvalidParameter,
  DuplicateIndex,
  DuplicateOrdinal,
  OverlappingParameters,
  ParameterOutOfBounds,
  InvalidPointerWidth,
  DuplicateKernel,
  UnknownKernel,
  ArgumentBlobSizeMismatch,
  MemoryTranslationFailed,
  ObjectTranslationFailed,
};

struct KernelMetadata {
  VirtualObject function = 0;
  std::string mangled_name;
  std::uint32_t argument_size = 0;
  std::uint16_t metadata_version = 1;
  MetadataSource source = MetadataSource::FatbinaryRegistration;
  std::vector<KernelParameterDescriptor> parameters;
};

struct PreparedKernelParameter {
  KernelParameterDescriptor descriptor{};
  std::vector<std::byte> value;
  std::uint64_t translated_identity = 0;
  bool managed_memory = false;
};

struct KernelMetadataResult {
  KernelMetadata metadata{};
  KernelMetadataError error = KernelMetadataError::None;
  explicit operator bool() const { return error == KernelMetadataError::None; }
};

struct LaunchPreparationResult {
  std::vector<PreparedKernelParameter> parameters;
  KernelMetadataError error = KernelMetadataError::None;
  explicit operator bool() const { return error == KernelMetadataError::None; }
};

class KernelMetadataRegistry {
 public:
  static constexpr std::size_t kMaxKernelNameBytes = 4096;
  static constexpr std::size_t kMaxParameters = 1024;

  explicit KernelMetadataRegistry(std::uint16_t session_namespace);

  KernelMetadataResult Register(const KernelMetadata &metadata,
                                const VirtualObjectRegistry &objects);
  KernelMetadataResult Lookup(VirtualObject function) const;
  KernelMetadataResult Release(VirtualObject function);

 private:
  KernelMetadataError ValidateMetadata(
      const KernelMetadata &metadata,
      const VirtualObjectRegistry &objects) const;

  const std::uint16_t session_namespace_;
  mutable std::mutex mutex_;
  std::map<VirtualObject, KernelMetadata> kernels_;
};

LaunchPreparationResult PrepareKernelArguments(
    const KernelMetadata &metadata, const std::byte *argument_blob,
    std::size_t argument_blob_size, const VirtualMemoryRegistry &memory,
    const VirtualObjectRegistry &objects);

const char *ToString(KernelMetadataError error);

}  // namespace gvirtus::protocol::v2
