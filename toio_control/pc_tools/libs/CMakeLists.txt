set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(OpenSSL REQUIRED)

add_library(toio_control_osc STATIC
    src/OscPacket.cpp
    src/OscTransport.cpp
    src/DeviceRegistry.cpp
    src/OscEncryptor.cpp
)

target_include_directories(toio_control_osc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_link_libraries(toio_control_osc
    PUBLIC
        asio::asio
        spdlog::spdlog
        fmt::fmt
        OpenSSL::Crypto
)

target_compile_definitions(toio_control_osc
    PUBLIC
        ASIO_STANDALONE
)

target_compile_features(toio_control_osc PUBLIC cxx_std_17)
