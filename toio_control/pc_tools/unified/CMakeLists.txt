cmake_minimum_required(VERSION 3.10)
project(toio_control_unified)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Swarm control sources (整理後の構造)
set(SWARM_SOURCES
    ../swarm_control/src/config/params.cpp
    ../swarm_control/src/config/config.cpp
    ../swarm_control/src/utils/utils.cpp
    ../swarm_control/src/utils/device/device_manager.cpp
    ../swarm_control/src/algorithm/spatial/spatial_density_grid.cpp
    ../swarm_control/src/algorithm/flocking/cluster_detector.cpp
    ../swarm_control/src/algorithm/collision/predictive_avoidance.cpp
    ../swarm_control/src/algorithm/spatial/spot_potential.cpp
    ../swarm_control/src/algorithm/collision/urgent_escape.cpp
    ../swarm_control/src/algorithm/flocking/boid_model.cpp
    ../swarm_control/src/algorithm/agent/robot_agent.cpp
    ../swarm_control/src/comm/osc_sender.cpp
    ../swarm_control/src/comm/osc_receiver.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../swarm_control  # 相対パス（../config/など）を解決するため
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../scheduler/include
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

