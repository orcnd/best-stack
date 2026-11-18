find_package(Drogon CONFIG REQUIRED)
add_executable(bs-drogon main.cpp)
target_link_libraries(bs-drogon PRIVATE bs_core Drogon::Drogon)
