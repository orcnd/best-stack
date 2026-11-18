set(CMAKE_POLICY_VERSION_MINIMUM 3.5)  # oatpp 1.3.1 declares cmake_minimum_required < 3.5
FetchContent_Declare(oatpp
  GIT_REPOSITORY https://github.com/oatpp/oatpp.git
  GIT_TAG 1.3.1
  GIT_SHALLOW TRUE)
set(OATPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OATPP_INSTALL OFF CACHE BOOL "" FORCE)
set(OATPP_LINK_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(oatpp)
add_executable(bs-oatpp main.cpp)
target_link_libraries(bs-oatpp PRIVATE bs_core oatpp)
