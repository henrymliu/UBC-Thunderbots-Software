package(default_visibility = ["//software/ai:__subpackages__"])

cc_library(
    name = "path_planner",
    hdrs = ["path_planner.h"],
    deps = [
        "//software/ai/navigator/obstacle",
        "//software/geom",
    ],
)

cc_library(
    name = "straight_line_path_planner",
    srcs = ["straight_line_path_planner.cpp"],
    hdrs = ["straight_line_path_planner.h"],
    deps = [":path_planner"],
)

cc_test(
    name = "straight_line_path_planner_test",
    srcs = ["straight_line_path_planner_test.cpp"],
    deps = [
        ":straight_line_path_planner",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "theta_star_path_planner",
    srcs = ["theta_star_path_planner.cpp"],
    hdrs = ["theta_star_path_planner.h"],
    deps = [
        ":path_planner",
    ],
)

cc_library(
    name = "no_path_test_path_planner",
    srcs = ["no_path_test_path_planner.cpp"],
    hdrs = ["no_path_test_path_planner.h"],
    deps = [
        ":path_planner",
    ],
)

cc_library(
    name = "one_point_path_test_path_planner",
    srcs = ["one_point_path_test_path_planner.cpp"],
    hdrs = ["one_point_path_test_path_planner.h"],
    deps = [
        ":path_planner",
    ],
)

cc_test(
    name = "theta_star_path_planner_test",
    srcs = ["theta_star_path_planner_test.cpp"],
    deps = [
        ":theta_star_path_planner",
        "//software/world:field",
        "//software/test_util",
        "@gtest//:gtest_main",
    ],
)
