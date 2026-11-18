FetchContent_Declare(uwebsockets
  GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
  GIT_TAG v20.81.0
  GIT_SHALLOW TRUE
  GIT_SUBMODULES "")
# uSockets fetched separately without its own (large, SSL/QUIC) submodules.
FetchContent_Declare(usockets
  GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
  GIT_TAG 86097c490263ab662d62e8e7b541390bdec7d149  # commit pinned by uWebSockets v20.81.0
  GIT_SUBMODULES "")
FetchContent_GetProperties(uwebsockets)
if(NOT uwebsockets_POPULATED)
  FetchContent_Populate(uwebsockets)
endif()
FetchContent_GetProperties(usockets)
if(NOT usockets_POPULATED)
  FetchContent_Populate(usockets)
endif()
set(USOCKETS_DIR ${usockets_SOURCE_DIR})
file(GLOB USOCKETS_SOURCES ${USOCKETS_DIR}/src/*.c ${USOCKETS_DIR}/src/eventing/*.c ${USOCKETS_DIR}/src/crypto/*.c)
add_library(usockets STATIC ${USOCKETS_SOURCES})
target_include_directories(usockets PUBLIC ${USOCKETS_DIR}/src)
target_compile_definitions(usockets PUBLIC LIBUS_NO_SSL)
add_executable(bs-uwebsockets main.cpp)
target_include_directories(bs-uwebsockets PRIVATE ${uwebsockets_SOURCE_DIR}/src)
target_compile_definitions(bs-uwebsockets PRIVATE UWS_NO_ZLIB)
target_link_libraries(bs-uwebsockets PRIVATE bs_core usockets)
