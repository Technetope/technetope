add_library(toio_control_scheduler STATIC
    src/SchedulerController.cpp
    src/SoundTimeline.cpp
    src/TargetResolver.cpp
)

target_include_directories(toio_control_scheduler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_link_libraries(toio_control_scheduler
    PUBLIC
        toio_control_osc
)

add_executable(agent_a_scheduler
    src/main.cpp
)

target_include_directories(agent_a_scheduler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_link_libraries(agent_a_scheduler
    PRIVATE
        toio_control_scheduler
)

if (WIN32)
    target_link_libraries(agent_a_scheduler PRIVATE ws2_32)
endif()

if (BUILD_TESTING)
    add_executable(toio_control_scheduler_tests
        tests/SoundTimelineTests.cpp
        tests/OscEncryptorTests.cpp
    )

    target_link_libraries(toio_control_scheduler_tests
        PRIVATE
            toio_control_scheduler
            Catch2::Catch2WithMain
    )

    add_test(NAME toio_control_scheduler_tests COMMAND toio_control_scheduler_tests)
endif()
