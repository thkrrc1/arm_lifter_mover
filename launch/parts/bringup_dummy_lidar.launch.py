import os
from ament_index_python.packages import get_package_share_directory
from ament_index_python.packages import get_package_share_path

import launch
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler
from launch.conditions import IfCondition, UnlessCondition
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.descriptions import ComposableNode, ParameterFile
from nav2_common.launch import RewrittenYaml
import launch_ros.actions
from launch_ros.substitutions import FindPackageShare
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.actions import Node
import xacro
import shutil
import yaml


def bringup_dummy_lidar(dummy_map_file, description):
    lidar1 = Node(
        package="dummy_scan",
        executable="dummy_scan",
        output="screen",
        remappings=[('/scan', '/scan_raw')],
        )

    scan_map = Node(
                package='nav2_map_server',
                executable='map_server',
                name='scan_map_server',
                output='screen',
                parameters=[{'yaml_filename': dummy_map_file}],
                remappings=[('/map', '/scan_map')])
    lifecycle = Node(
                 package='nav2_lifecycle_manager',
                 executable='lifecycle_manager',
                 name='lifecycle_manager_scan',
                 output='log',
                 parameters=[{'use_sim_time': False},
                             {'autostart': True},
                             {'node_names': ['scan_map_server']}])

    description.add_action(lidar1)
    description.add_action(scan_map)
    description.add_action(lifecycle)
    
    description.add_action(Node(
            package="laser_filters",
            executable="scan_to_scan_filter_chain",
            name='front_laser_filter',
            parameters=[
                PathJoinSubstitution([
                    get_package_share_directory("arm_lifter_mover"),
                    "config","laser", "laser_filter.yaml",
                ])],
            remappings=[('/scan', '/scan_raw'),('/scan_filtered', '/scan')],
        )
    )
    

def generate_launch_description():
    dummy_map_file = LaunchConfiguration("dummy_map")
    dummy_map_file_arg = DeclareLaunchArgument("dummy_map")

    ld = LaunchDescription()
    ld.add_action(dummy_map_file_arg)
    
    bringup_dummy_lidar(dummy_map_file, ld)

    return ld
