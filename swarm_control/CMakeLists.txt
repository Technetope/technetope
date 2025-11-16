cmake_minimum_required(VERSION 3.10)
project(swarm_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースファイル
set(SOURCES
    src/params.cpp
    src/utils.cpp
    src/spatial_density_grid.cpp
    src/cluster_detector.cpp
    src/predictive_avoidance.cpp
    src/spot_potential.cpp
    src/urgent_escape.cpp
    src/boid_model.cpp
    src/robot_agent.cpp
    src/osc_sender.cpp
    src/osc_receiver.cpp
    src/device_manager.cpp
    src/config.cpp
    src/main.cpp
)

# ヘッダーファイル
set(HEADERS
    src/params.hpp
    src/utils.hpp
    src/human_spot.hpp
    src/spatial_density_grid.hpp
    src/cluster_detector.hpp
    src/predictive_avoidance.hpp
    src/spot_potential.hpp
    src/urgent_escape.hpp
    src/boid_model.hpp
    src/robot_agent.hpp
    src/osc_sender.hpp
    src/osc_receiver.hpp
    src/device_manager.hpp
    src/config.hpp
)

# 実行ファイル
add_executable(swarm_control ${SOURCES} ${HEADERS})

# インクルードディレクトリ
target_include_directories(swarm_control
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../acoustics/pc_tools/libs/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../acoustics/pc_tools/libs/../third_party  # json.hpp
)

# 依存ライブラリ
# acoustics_oscライブラリをリンク（既存のビルドシステムから）
# まず、acoustics_oscライブラリのパスを探す
set(ACOUSTICS_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../acoustics/pc_tools/libs")
set(ACOUSTICS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../acoustics/build")

# acoustics_oscライブラリを直接ビルドするか、既存のものを探す
if(EXISTS "${ACOUSTICS_BUILD_DIR}/libs/libacoustics_osc.a")
    target_link_libraries(swarm_control "${ACOUSTICS_BUILD_DIR}/libs/libacoustics_osc.a")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/acoustics/libs/libacoustics_osc.a")
    target_link_libraries(swarm_control "${CMAKE_CURRENT_SOURCE_DIR}/../build/acoustics/libs/libacoustics_osc.a")
else()
    message(WARNING "acoustics_osc library not found. Please build acoustics project first.")
endif()

# 標準ライブラリ
target_link_libraries(swarm_control
    PRIVATE
        asio::asio
        spdlog::spdlog
        fmt::fmt
        OpenSSL::Crypto
)

# コンパイル定義
target_compile_definitions(swarm_control
    PRIVATE
        ASIO_STANDALONE
)

# コンパイルオプション
target_compile_options(swarm_control
    PRIVATE
        -Wall
        -Wextra
)

