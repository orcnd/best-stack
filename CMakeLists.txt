cmake_minimum_required(VERSION 3.28)
project(best_stack LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

set(BS_BACKENDS "httplib;drogon;crow;oatpp;uwebsockets;crow-opt;uwebsockets-opt" CACHE STRING "Backends to build")

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# Pinned versions: keep in sync with shared/versions.json
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
set(INJA_USE_EMBEDDED_JSON OFF CACHE BOOL "" FORCE)
set(INJA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(INJA_INSTALL OFF CACHE BOOL "" FORCE)
set(INJA_EXPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(inja
  GIT_REPOSITORY https://github.com/pantor/inja.git
  GIT_TAG v3.5.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json inja doctest)

# SQLite built from the pinned amalgamation. Homebrew's build has ENABLE_MEMORY_MANAGEMENT, which puts
# every connection's page cache behind one global mutex (concurrent readers serialized: 21 → 10 req/s
# going from 1 to 8 clients on /api/dashboard). Options follow sqlite.org's recommended set.
FetchContent_Declare(sqlite_amalgamation
  URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip)
FetchContent_MakeAvailable(sqlite_amalgamation)
add_library(sqlite3_static STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_static PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
target_compile_definitions(sqlite3_static PUBLIC
  SQLITE_THREADSAFE=2 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1 SQLITE_DQS=0
  SQLITE_LIKE_DOESNT_MATCH_BLOBS SQLITE_OMIT_DEPRECATED SQLITE_OMIT_SHARED_CACHE SQLITE_USE_ALLOCA)
set(SQLITE3_INCLUDE_DIR ${sqlite_amalgamation_SOURCE_DIR})
set(SQLITE3_LIBRARY sqlite3_static)

file(GLOB BS_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shared/cpp/src/*.cpp)
add_library(bs_core STATIC ${BS_CORE_SOURCES})
target_include_directories(bs_core PUBLIC ${CMAKE_SOURCE_DIR}/shared/cpp/include ${SQLITE3_INCLUDE_DIR})
target_link_libraries(bs_core PUBLIC nlohmann_json::nlohmann_json pantor::inja ${SQLITE3_LIBRARY})

# Run 3 "fast" core (crow-opt, uwebsockets-opt): same DB layer, glaze for JSON, HTML written directly into
# one reserved buffer per response, mimalloc overriding malloc/new for the whole process.
if(BS_BACKENDS MATCHES "-opt")
  FetchContent_Declare(glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG v9.1.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)
  set(glaze_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
  set(glaze_ENABLE_FUZZING OFF CACHE BOOL "" FORCE)
  set(MI_OVERRIDE ON CACHE BOOL "" FORCE)
  set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_STATIC ON CACHE BOOL "" FORCE)  # mimalloc's object-copy target depends on it
  set(MI_BUILD_OBJECT ON CACHE BOOL "" FORCE)
  set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MI_INSTALL_TOPLEVEL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG v3.7.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(glaze mimalloc)

  # Non-rendering pieces of bs_core are compiled unchanged (same SQL, same SQLite flags); rendering
  # (view.cpp, render.cpp, json.cpp, handlers_*.cpp) is replaced by shared/cpp-fast.
  file(GLOB BS_FAST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shared/cpp-fast/src/*.cpp)
  add_library(bs_core_fast STATIC ${BS_FAST_SOURCES}
    ${CMAKE_SOURCE_DIR}/shared/cpp/src/db.cpp
    ${CMAKE_SOURCE_DIR}/shared/cpp/src/http.cpp
    ${CMAKE_SOURCE_DIR}/shared/cpp/src/version.cpp)
  target_include_directories(bs_core_fast PUBLIC ${CMAKE_SOURCE_DIR}/shared/cpp-fast/include
    ${CMAKE_SOURCE_DIR}/shared/cpp/include ${SQLITE3_INCLUDE_DIR})
  # nlohmann is header-only and only needed because http.cpp includes bs/render.hpp; nothing in the
  # request path uses it.
  # glaze is PRIVATE: only json.cpp uses it, and backends' main.cpp keep the C++20 build of the originals
  # (glaze itself needs C++23).
  target_link_libraries(bs_core_fast PRIVATE glaze::glaze)
  target_link_libraries(bs_core_fast PUBLIC nlohmann_json::nlohmann_json ${SQLITE3_LIBRARY})
  target_include_directories(bs_core_fast PUBLIC ${mimalloc_SOURCE_DIR}/include)
  # The mimalloc object file (not the archive) so the malloc/new overrides are always linked in.
  set(BS_MIMALLOC_OBJ $<TARGET_OBJECTS:mimalloc-obj>)

  file(GLOB BS_FAST_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shared/cpp-fast/tests/*.cpp)
  add_executable(bs_fast_tests ${BS_FAST_TEST_SOURCES} ${BS_MIMALLOC_OBJ})
  target_link_libraries(bs_fast_tests PRIVATE bs_core_fast doctest::doctest)
  target_compile_definitions(bs_fast_tests PRIVATE BS_ROOT="${CMAKE_SOURCE_DIR}")
endif()

enable_testing()
file(GLOB BS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shared/cpp/tests/*.cpp)
add_executable(bs_tests ${BS_TEST_SOURCES})
target_link_libraries(bs_tests PRIVATE bs_core doctest::doctest)
target_compile_definitions(bs_tests PRIVATE BS_ROOT="${CMAKE_SOURCE_DIR}")
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(bs_tests)
if(TARGET bs_fast_tests)
  doctest_discover_tests(bs_fast_tests)
endif()

foreach(be IN LISTS BS_BACKENDS)
  if(EXISTS ${CMAKE_SOURCE_DIR}/backends/${be}/CMakeLists.txt)
    add_subdirectory(backends/${be})
  endif()
endforeach()

# Measurement helper (bench/harness/sampler.ts)
add_executable(bs-footprint bench/tools/footprint.c)
