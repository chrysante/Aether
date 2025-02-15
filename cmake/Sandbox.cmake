add_executable(Sandbox MACOSX_BUNDLE)

target_sources(Sandbox
PRIVATE
    src/Sandbox/Sandbox.cpp
)
target_link_libraries(Sandbox
    PRIVATE Aether Flow
    PRIVATE WarningFlags Sanitizers
)

set(EXECUTABLE_NAME "Sandbox")
configure_file("${CMAKE_SOURCE_DIR}/resource/Info.plist.in"
               "${CMAKE_BINARY_DIR}/resource/Sandbox/Info.plist" @ONLY)

set_target_properties(Sandbox PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/resource/Sandbox/Info.plist"
)
