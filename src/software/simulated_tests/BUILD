package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mock_ai",
    srcs = ["mock_ai_wrapper.cpp"],
    hdrs = ["mock_ai_wrapper.h"],
    deps = [
        "//software/ai/primitive",
        "//software/multithreading:subject",
        "//software/multithreading:threaded_observer",
        "//software/world",
    ],
)

cc_library(
    name = "simulated_test_fixture",
    srcs = ["simulated_test_fixture.cpp"],
    hdrs = ["simulated_test_fixture.h"],
    deps = [
        "//software/ai:ai_wrapper",
        "//software/backend:simulator_backend",
        "//software/simulated_tests/validation:world_state_validator",
        "//software/util/logger",
        "//software/util/time:duration",
        "//software/visualizer:visualizer_wrapper",
        "@gtest",
    ],
)

cc_test(
    name = "simulated_tests_test",
    srcs = ["simulated_tests_test.cpp"],
    deps = [
        ":mock_ai",
        ":simulated_test_fixture",
        "//software/simulated_tests/validation:validation_function",
        "//software/test_util",
        "//software/util/time:duration",
        "//software/util/time:timestamp",
        "//software/world",
        "@gtest//:gtest_main",
    ],
)
