FetchContent_Declare(crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.3.5
  GIT_SHALLOW TRUE)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)
add_executable(bs-crow main.cpp)
target_compile_definitions(bs-crow PRIVATE CROW_DISABLE_STATIC_DIR)
target_link_libraries(bs-crow PRIVATE bs_core Crow::Crow)
