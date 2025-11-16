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
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

# 依存ライブラリ
target_link_libraries(swarm_control
    PRIVATE
        toio_control_osc
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

