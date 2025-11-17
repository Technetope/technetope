cmake_minimum_required(VERSION 3.10)
project(swarm_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースファイル
set(SOURCES
    src/config/params.cpp
    src/config/config.cpp
    src/utils/utils.cpp
    src/utils/device/device_manager.cpp
    src/algorithm/spatial/spatial_density_grid.cpp
    src/algorithm/flocking/cluster_detector.cpp
    src/algorithm/collision/predictive_avoidance.cpp
    src/algorithm/spatial/spot_potential.cpp
    src/algorithm/collision/urgent_escape.cpp
    src/algorithm/flocking/boid_model.cpp
    src/algorithm/agent/robot_agent.cpp
    src/comm/osc_sender.cpp
    src/comm/osc_receiver.cpp
    src/main.cpp
)

# ヘッダーファイル
set(HEADERS
    src/config/params.hpp
    src/config/config.hpp
    src/utils/utils.hpp
    src/utils/types/human_spot.hpp
    src/utils/types/position.hpp
    src/utils/types/velocity.hpp
    src/utils/device/device_manager.hpp
    src/algorithm/spatial/spatial_density_grid.hpp
    src/algorithm/flocking/cluster_detector.hpp
    src/algorithm/collision/predictive_avoidance.hpp
    src/algorithm/spatial/spot_potential.hpp
    src/algorithm/collision/urgent_escape.hpp
    src/algorithm/flocking/boid_model.hpp
    src/algorithm/agent/robot_agent.hpp
    src/comm/osc_sender.hpp
    src/comm/osc_receiver.hpp
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

