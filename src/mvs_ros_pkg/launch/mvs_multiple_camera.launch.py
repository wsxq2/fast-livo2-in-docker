"""
mvs_multiple_camera.launch.py  —  双相机同时启动
等价于原版 ROS1 mvs_multiple_camera.launch

用法:
  ros2 launch mvs_ros_pkg mvs_multiple_camera.launch.py
  ros2 launch mvs_ros_pkg mvs_multiple_camera.launch.py \
      left_config:=/path/to/left.yaml right_config:=/path/to/right.yaml
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('mvs_ros_pkg')

    left_default  = os.path.join(pkg_share, 'config', 'left_camera_trigger.yaml')
    right_default = os.path.join(pkg_share, 'config', 'right_camera_trigger.yaml')

    left_config_arg = DeclareLaunchArgument(
        'left_config',
        default_value=left_default,
        description='左相机配置文件路径'
    )
    right_config_arg = DeclareLaunchArgument(
        'right_config',
        default_value=right_default,
        description='右相机配置文件路径'
    )

    left_node = Node(
        package='mvs_ros_pkg',
        executable='mvs_trigger',
        name='left_camera',
        output='screen',
        arguments=[LaunchConfiguration('left_config')],
        respawn=True,
        respawn_delay=2.0,
    )

    right_node = Node(
        package='mvs_ros_pkg',
        executable='mvs_trigger',
        name='right_camera',
        output='screen',
        arguments=[LaunchConfiguration('right_config')],
        respawn=True,
        respawn_delay=2.0,
    )

    return LaunchDescription([
        left_config_arg,
        right_config_arg,
        left_node,
        right_node,
    ])
