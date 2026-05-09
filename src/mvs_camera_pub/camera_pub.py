#!/usr/bin/env python3
"""
海康相机 ROS2 发布节点
话题: /left_camera/image (sensor_msgs/Image, bgr8)

x86_64 使用方式:
  source /opt/ros/humble/setup.bash
  LD_PRELOAD=/lib/x86_64-linux-gnu/libusb-1.0.so.0 python3 camera_ros2_pub.py

aarch64 (树莓派) 使用方式:
  source /opt/ros/humble/setup.bash
  python3 camera_ros2_pub.py
"""
import sys
import os
import platform
import ctypes
import numpy as np
import cv2

# 根据架构自动选择 MVS SDK Python 路径
_arch = platform.machine()
if _arch == 'aarch64':
    sys.path.append('/opt/MVS/Samples/aarch64/Python/MvImport')
else:
    sys.path.append('/opt/MVS/Samples/64/Python/MvImport')
from MvCameraControl_class import *

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge


class MvsCameraPublisher(Node):
    def __init__(self):
        super().__init__('mvs_camera_publisher')

        self.pub = self.create_publisher(Image, '/left_camera/image', 10)
        self.bridge = CvBridge()

        # 枚举设备
        deviceList = MV_CC_DEVICE_INFO_LIST()
        ret = MvCamera.MV_CC_EnumDevices(MV_USB_DEVICE, deviceList)
        if ret != 0 or deviceList.nDeviceNum == 0:
            self.get_logger().error(f'未找到USB相机, ret={hex(ret)}')
            sys.exit(1)
        self.get_logger().info(f'找到 {deviceList.nDeviceNum} 台相机')

        # 打开第一台相机
        self.cam = MvCamera()
        stDeviceList = cast(deviceList.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents
        ret  = self.cam.MV_CC_CreateHandle(stDeviceList)
        ret |= self.cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0)
        ret |= self.cam.MV_CC_SetEnumValue("TriggerMode", MV_TRIGGER_MODE_OFF)
        ret |= self.cam.MV_CC_StartGrabbing()
        if ret != 0:
            self.get_logger().error(f'相机初始化失败, ret={hex(ret)}')
            sys.exit(1)

        self.get_logger().info('相机已启动，开始发布 /left_camera/image')

        # 预分配缓冲区（按实际最大分辨率 1280×1024）
        self.buf = (ctypes.c_ubyte * (1280 * 1024 * 3))()
        self.stFrameInfo = MV_FRAME_OUT_INFO_EX()

        # 10Hz 定时发布
        self.timer = self.create_timer(0.1, self.publish_frame)

    def publish_frame(self):
        ret = self.cam.MV_CC_GetOneFrameTimeout(
            self.buf, ctypes.sizeof(self.buf), self.stFrameInfo, 1000)
        if ret != 0:
            return

        w = self.stFrameInfo.nWidth
        h = self.stFrameInfo.nHeight
        data = np.frombuffer(self.buf, dtype=np.uint8)

        pixel_format = self.stFrameInfo.enPixelType
        if pixel_format == PixelType_Gvsp_BayerRG8:
            raw = data[:h * w].reshape(h, w)
            bgr = cv2.cvtColor(raw, cv2.COLOR_BayerRG2BGR)
        elif pixel_format == PixelType_Gvsp_BayerGB8:
            raw = data[:h * w].reshape(h, w)
            bgr = cv2.cvtColor(raw, cv2.COLOR_BayerGB2BGR)
        elif pixel_format == PixelType_Gvsp_BayerBG8:
            raw = data[:h * w].reshape(h, w)
            bgr = cv2.cvtColor(raw, cv2.COLOR_BayerBG2BGR)
        elif pixel_format == PixelType_Gvsp_BayerGR8:
            raw = data[:h * w].reshape(h, w)
            bgr = cv2.cvtColor(raw, cv2.COLOR_BayerGR2BGR)
        elif pixel_format == PixelType_Gvsp_RGB8_Packed:
            raw = data[:h * w * 3].reshape(h, w, 3)
            bgr = cv2.cvtColor(raw, cv2.COLOR_RGB2BGR)
        else:
            # 默认尝试 BGR
            bgr = data[:h * w * 3].reshape(h, w, 3)

        # 降采样到算法实际使用的分辨率 640×512（scale=0.5）
        bgr = cv2.resize(bgr, (640, 512), interpolation=cv2.INTER_LINEAR)

        msg = self.bridge.cv2_to_imgmsg(bgr, encoding='bgr8')
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = 'camera'
        self.pub.publish(msg)

    def destroy_node(self):
        self.cam.MV_CC_StopGrabbing()
        self.cam.MV_CC_CloseDevice()
        self.cam.MV_CC_DestroyHandle()
        super().destroy_node()


def main():
    rclpy.init()
    node = MvsCameraPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
