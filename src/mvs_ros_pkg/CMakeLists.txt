cmake_minimum_required(VERSION 3.8)
project(mvs_ros_pkg)

# 要求 C++17
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
set(CMAKE_CXX_STANDARD 17)

# ── ROS2 依赖 ──────────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)

# ── 海康 MVS SDK ──────────────────────────────────────────────────────────
# 默认安装路径：/opt/MVS；可通过 -DMVS_ROOT=/path/to/mvs 覆盖
set(MVS_ROOT "/opt/MVS" CACHE PATH "海康 MVS SDK 根目录")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(MVS_LIB_DIR "${MVS_ROOT}/lib/aarch64")
else()
  set(MVS_LIB_DIR "${MVS_ROOT}/lib/64")
endif()

find_path(MVS_INCLUDE_DIR
  NAMES MvCameraControl.h
  PATHS "${MVS_ROOT}/include"
  REQUIRED
)
find_library(MVS_LIBRARY
  NAMES MvCameraControl
  PATHS "${MVS_LIB_DIR}"
  REQUIRED
)

message(STATUS "MVS include : ${MVS_INCLUDE_DIR}")
message(STATUS "MVS library : ${MVS_LIBRARY}")

# ── 可执行文件 ─────────────────────────────────────────────────────────────
add_executable(mvs_trigger src/grab_trigger.cpp)

target_include_directories(mvs_trigger PRIVATE
  ${MVS_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(mvs_trigger
  ${MVS_LIBRARY}
  ${OpenCV_LIBS}
  pthread
)

ament_target_dependencies(mvs_trigger
  rclcpp
  image_transport
  cv_bridge
  sensor_msgs
  std_msgs
)

# 让可执行文件在运行时能找到 MVS 共享库（无需额外设置 LD_LIBRARY_PATH）
set_target_properties(mvs_trigger PROPERTIES
  INSTALL_RPATH "${MVS_LIB_DIR}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# ── 安装 ───────────────────────────────────────────────────────────────────
install(TARGETS mvs_trigger
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

ament_package()
