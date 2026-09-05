add_executable(gvirtus-managed-memory-coherence-example
        coherence_walkthrough.cpp)
target_link_libraries(gvirtus-managed-memory-coherence-example PRIVATE
        gvirtus-protocol-v2)
