cmake_minimum_required(VERSION 3.10)
project(toio_control_unified)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Swarm control sources (excluding main.cpp)
set(SWARM_SOURCES
    ../swarm_control/src/params.cpp
    ../swarm_control/src/utils.cpp
    ../swarm_control/src/spatial_density_grid.cpp
    ../swarm_control/src/cluster_detector.cpp
    ../swarm_control/src/predictive_avoidance.cpp
    ../swarm_control/src/spot_potential.cpp
    ../swarm_control/src/urgent_escape.cpp
    ../swarm_control/src/boid_model.cpp
    ../swarm_control/src/robot_agent.cpp
    ../swarm_control/src/osc_sender.cpp
    ../swarm_control/src/osc_receiver.cpp
    ../swarm_control/src/device_manager.cpp
)

# ソースファイル
set(SOURCES
    src/main.cpp
    src/unified_config.cpp
    ${SWARM_SOURCES}
)

# 実行ファイル
add_executable(toio_control ${SOURCES})

# インクルードディレクトリ
target_include_directories(toio_control
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../swarm_control/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

# 依存ライブラリ
target_link_libraries(toio_control
    PRIVATE
        toio_control_osc
        toio_control_scheduler
        asio::asio
        spdlog::spdlog
        fmt::fmt
        OpenSSL::Crypto
)

# コンパイル定義
target_compile_definitions(toio_control
    PRIVATE
        ASIO_STANDALONE
)

# コンパイルオプション
target_compile_options(toio_control
    PRIVATE
        -Wall
        -Wextra
)

