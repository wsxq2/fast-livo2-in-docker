"""
mvs_camera_trigger.launch.py  —  单相机启动文件
等价于原版 ROS1 mvs_camera_trigger.launch
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_share = get_package_share_directory('mvs_ros_pkg')

    default_config = os.path.join(pkg_share, 'config', 'left_camera_trigger.yaml')

    config_arg = DeclareLaunchArgument(
        'config',
        default_value=default_config,
        description='相机配置文件路径（OpenCV YAML 格式）'
    )

    mvs_node = Node(
        package='mvs_ros_pkg',
        executable='mvs_trigger',
        name='mvs_camera_trigger',
        output='screen',
        arguments=[LaunchConfiguration('config')],
        respawn=True,
        respawn_delay=2.0,
    )

    return LaunchDescription([
        config_arg,
        mvs_node,
    ])
