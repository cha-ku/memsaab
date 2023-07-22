add_executable(cppmemsaab main.cpp)

target_link_system_libraries(
    cppmemsaab
    PRIVATE
            CLI11::CLI11
            fmt::fmt
            spdlog::spdlog
)

# target_include_directories(cppmemsaab PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")