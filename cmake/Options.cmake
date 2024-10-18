
add_library(WarningFlags INTERFACE)

target_compile_options(WarningFlags INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /WX>
)

add_library(Sanitizers INTERFACE)

target_compile_options(Sanitizers INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -fsanitize=undefined,address>
)

target_link_options(Sanitizers INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -fsanitize=undefined,address>
)
