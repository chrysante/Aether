
set(SOURCE_FILES
    src/Flow/Editor.cpp
)
set(HEADER_FILES
    include/Flow/Editor.h
    include/Flow/Node.h
    include/Flow/Graph.h
)

add_library(Flow SHARED)

target_sources(Flow
    PRIVATE ${HEADER_FILES}
    PRIVATE ${SOURCE_FILES})

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(Flow
  PUBLIC
    Aether
  PRIVATE
    WarningFlags Sanitizers utility
)
