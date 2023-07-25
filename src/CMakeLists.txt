add_executable(cppmemsaab main.cpp server.hpp server.cpp)
target_link_system_libraries(
    cppmemsaab
    PRIVATE
    CLI11::CLI11
    fmt::fmt
    spdlog::spdlog
    uvw::uvw
)

target_include_directories(cppmemsaab PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")