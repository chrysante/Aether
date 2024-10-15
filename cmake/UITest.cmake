add_executable(UITest MACOSX_BUNDLE)

target_sources(UITest PRIVATE
    src/UITest/UITest.cpp
)
target_link_libraries(UITest
    PRIVATE Aether
    PRIVATE WarningFlags
)

set(EXECUTABLE_NAME "UITest")
configure_file("${CMAKE_SOURCE_DIR}/resource/Info.plist.in"
               "${CMAKE_BINARY_DIR}/resource/UITest/Info.plist" @ONLY)

set_target_properties(UITest PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/resource/UITest/Info.plist"
)
