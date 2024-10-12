
add_library(WarningFlags INTERFACE)

target_compile_options(WarningFlags INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /WX>
)
