add_executable(run_tests
    TestHarness.cpp
    tests_alu.cpp
    tests_loadstore.cpp
    tests_branch_jump.cpp
    tests_programs.cpp
    tests_edge_cases.cpp
    tests_coverage_closure.cpp
    tests_random.cpp
    tests_decoder.cpp
    run_tests.cpp
)
target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(run_tests PRIVATE rv32i_core)

add_test(NAME run_tests COMMAND run_tests --out-dir ${CMAKE_BINARY_DIR}/out)
