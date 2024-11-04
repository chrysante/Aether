cmake_minimum_required(VERSION 3.10)

project(Aether VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(cmake/CPM.cmake)

# To make range-v3 happy
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")
CPMAddPackage("gh:chrysante/csp#0.0.1")
CPMAddPackage("gh:chrysante/utility#main")
CPMAddPackage("gh:chrysante/vml#main")

include(cmake/Options.cmake)
include(cmake/Aether.cmake)
include(cmake/UITest.cmake)
include(cmake/Sandbox.cmake)
include(cmake/Flow.cmake)
