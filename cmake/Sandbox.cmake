
add_executable(Sandbox)

target_sources(Sandbox PRIVATE
    src/Sandbox/Sandbox.cpp
)
target_link_libraries(Sandbox PRIVATE Aether)
