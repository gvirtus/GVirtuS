# CI/CD

GVirtuS uses focused GitHub Actions workflows rather than treating all build
configurations as equivalent.

## Continuous integration

`ci.yml` runs for pull requests and pushes to `master` and `development`. It
configures the CPU-only, TCP-only build with CUDA plugins and RDMA disabled,
then builds and runs the TCP communicator regression test with GCC and Clang in
Debug and Release modes. It also rejects whitespace errors in changed lines.

`nightly.yml` runs the same hardware-independent regression under AddressSanitizer
and UndefinedBehaviorSanitizer. It can also be started manually.

These jobs do not constitute CUDA, UCX, InfiniBand, RDMA, or GPUDirect
integration coverage. Those configurations require separately labelled
self-hosted runners with the applicable hardware and drivers.

## Continuous delivery

`release.yml` runs on tags beginning with `v`. It first executes the CPU/TCP
release validation, then creates a source archive and SHA-256 checksum and
publishes them to the corresponding GitHub release. A manual run validates and
uploads workflow artifacts but does not publish a GitHub release.

Creating a tag does not certify CUDA integration. A stable release should only
be tagged after the required CUDA-capable validation has been completed and
recorded separately.

Dependabot checks GitHub Actions dependencies weekly.
