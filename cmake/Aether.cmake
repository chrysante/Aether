set(SOURCE_FILES
    src/Aether/ADT.cpp
    src/Aether/Application.cpp
    src/Aether/Main.cpp
    src/Aether/Modifiers.cpp
    src/Aether/View.cpp
    src/Aether/ViewUtil.h
    src/Aether/Window.cpp
)
set(HEADER_FILES
    include/Aether/ADT.h
    include/Aether/Application.h
    include/Aether/Modifiers.h
    include/Aether/View.h
    include/Aether/ViewProperties.h
    include/Aether/Window.h
)

add_library(Aether SHARED)

if(APPLE)
    list(APPEND SOURCE_FILES
        src/Aether/MacOS/MacOSMain.mm
        src/Aether/MacOS/MacOSUtil.h
        src/Aether/MacOS/MacOSUtil.mm
        src/Aether/MacOS/MacOSView.mm
        src/Aether/MacOS/MacOSWindow.mm
    )
    target_compile_options(Aether PRIVATE -fobjc-arc)
    target_link_libraries(Aether PRIVATE 
        "-framework AppKit"
    )
endif() # APPLE

target_sources(Aether 
    PRIVATE ${HEADER_FILES}
    PRIVATE ${SOURCE_FILES})

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(Aether 
    PUBLIC include
    PRIVATE src
)

target_link_libraries(Aether
    PRIVATE range-v3
    PRIVATE WarningFlags
)
