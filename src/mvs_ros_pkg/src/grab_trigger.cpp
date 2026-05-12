/**
 * grab_trigger.cpp  —  海康 MVS 相机 ROS2 驱动（硬件触发 + 共享内存时间同步）
 *
 * 从 LIV_handhold (xuankuzcr) 的 ROS1 版本移植到 ROS2 (Humble)。
 *
 * 时间同步机制：
 *   STM32 在每次触发脉冲后，通过串口将 PC 时间戳（nanoseconds since epoch）
 *   写入共享内存文件 ~/timeshare（int64_t low 字段），驱动读取该值赋给图像帧头。
 *
 * 用法：
 *   ros2 run mvs_ros_pkg mvs_trigger <config_yaml_path>
 *   ros2 launch mvs_ros_pkg mvs_camera_trigger.launch.py
 */

#include "MvCameraControl.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>

#include <rclcpp/rclcpp.hpp>
#include <image_transport/image_transport.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/header.hpp>

using namespace std;

/* ── 共享内存时间戳结构体（与 STM32 侧保持一致） ── */
struct time_stamp {
  int64_t high;  // 预留（当前未使用）
  int64_t low;   // 触发时刻的 PC 纳秒时间戳
};
static time_stamp *pointt = nullptr;

/* ── 像素格式枚举 ── */
enum PixelFormat : unsigned int {
  RGB8           = 0x02180014,
  BayerRG8       = 0x01080009,
  BayerRG12Packed = 0x010C002B,
  BayerGB12Packed = 0x010C002C,
  BayerGB8       = 0x0108000A
};

/* ── 全局变量 ── */
static bool  exit_flag    = false;
static int   trigger_enable = 1;
static float image_scale  = 1.0f;

static image_transport::Publisher pub;
static rclcpp::Node::SharedPtr    g_node;

static const std::vector<unsigned int> PIXEL_FORMAT_LIST = {
  RGB8, BayerRG8, BayerRG12Packed, BayerGB12Packed, BayerGB8
};

static const std::string ExposureAutoStr[3] = {"Off", "Once", "Continues"};
static const std::string GammaSlectorStr[3] = {"User", "sRGB", "Off"};
static const std::string GainAutoStr[3]     = {"Off", "Once", "Continues"};

/* ════════════════════════════════════════════════════════════════════════════
 * PrintDeviceInfo  —  打印设备信息
 * ════════════════════════════════════════════════════════════════════════════ */
static bool PrintDeviceInfo(MV_CC_DEVICE_INFO *pstMVDevInfo)
{
  if (!pstMVDevInfo) {
    printf("The Pointer of pstMVDevInfo is NULL!\n");
    return false;
  }
  if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE) {
    int ip1 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp >> 24) & 0xFF;
    int ip2 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp >> 16) & 0xFF;
    int ip3 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp >>  8) & 0xFF;
    int ip4 =  pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp        & 0xFF;
    printf("  Model : %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
    printf("  IP    : %d.%d.%d.%d\n", ip1, ip2, ip3, ip4);
    printf("  SN    : %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chSerialNumber);
  } else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE) {
    printf("  Model : %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
    printf("  SN    : %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
  } else {
    printf("  (Not supported)\n");
  }
  return true;
}

/* ════════════════════════════════════════════════════════════════════════════
 * setParams  —  从 YAML 文件读取并配置相机参数
 * ════════════════════════════════════════════════════════════════════════════ */
static void setParams(void *handle, const std::string &params_file)
{
  cv::FileStorage fs(params_file, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "Failed to open settings file: %s", params_file.c_str());
    exit(-1);
  }

  image_scale = static_cast<float>(fs["image_scale"]);
  if (image_scale < 0.1f) image_scale = 1.0f;

  int   ExposureTimeLower = static_cast<int>(fs["AutoExposureTimeLower"]);
  int   ExposureTimeUpper = static_cast<int>(fs["AutoExposureTimeUpper"]);
  int   ExposureTime      = static_cast<int>(fs["ExposureTime"]);
  int   ExposureAutoMode  = static_cast<int>(fs["ExposureAutoMode"]);
  int   GainAuto          = static_cast<int>(fs["GainAuto"]);
  float Gain              = static_cast<float>(fs["Gain"]);
  float Gamma             = static_cast<float>(fs["Gamma"]);
  int   GammaSlector      = static_cast<int>(fs["GammaSelector"]);
  int   nRet;

  /* ── 曝光模式 ── */
  nRet = MV_CC_SetExposureAutoMode(handle, ExposureAutoMode);
  {
    std::string msg = "Set ExposureAutoMode: " + ExposureAutoStr[ExposureAutoMode];
    if (nRet == MV_OK) {
      RCLCPP_INFO(g_node->get_logger(), "%s", msg.c_str());
    } else if (ExposureAutoMode == 2) {
      RCLCPP_WARN(g_node->get_logger(), "Fail to set Exposure Auto Mode to Continues");
    } else {
      RCLCPP_INFO(g_node->get_logger(), "%s", msg.c_str());
    }
  }

  if (ExposureAutoMode == 2) {
    nRet = MV_CC_SetAutoExposureTimeLower(handle, ExposureTimeLower);
    if (nRet == MV_OK)
      RCLCPP_INFO(g_node->get_logger(), "Set Exposure Time Lower: %d us", ExposureTimeLower);
    else
      RCLCPP_ERROR(g_node->get_logger(), "Fail to set Exposure Time Lower");

    nRet = MV_CC_SetAutoExposureTimeUpper(handle, ExposureTimeUpper);
    if (nRet == MV_OK)
      RCLCPP_INFO(g_node->get_logger(), "Set Exposure Time Upper: %d us", ExposureTimeUpper);
    else
      RCLCPP_ERROR(g_node->get_logger(), "Fail to set Exposure Time Upper");
  }

  if (ExposureAutoMode == 0) {
    nRet = MV_CC_SetExposureTime(handle, ExposureTime);
    if (nRet == MV_OK)
      RCLCPP_INFO(g_node->get_logger(), "Set Exposure Time: %d us", ExposureTime);
    else
      RCLCPP_ERROR(g_node->get_logger(), "Fail to set Exposure Time");
  }

  /* ── 增益 ── */
  nRet = MV_CC_SetEnumValue(handle, "GainAuto", GainAuto);
  if (nRet == MV_OK)
    RCLCPP_INFO(g_node->get_logger(), "Set GainAuto: %s", GainAutoStr[GainAuto].c_str());
  else
    RCLCPP_ERROR(g_node->get_logger(), "Fail to set GainAuto");

  if (GainAuto == 0) {
    nRet = MV_CC_SetGain(handle, Gain);
    if (nRet == MV_OK)
      RCLCPP_INFO(g_node->get_logger(), "Set Gain: %.2f", Gain);
    else
      RCLCPP_ERROR(g_node->get_logger(), "Fail to set Gain");
  }

  /* ── Gamma ── */
  nRet = MV_CC_SetGammaSelector(handle, GammaSlector);
  if (nRet == MV_OK)
    RCLCPP_INFO(g_node->get_logger(), "Set GammaSelector: %s", GammaSlectorStr[GammaSlector].c_str());
  else
    RCLCPP_ERROR(g_node->get_logger(), "Fail to set GammaSelector");

  nRet = MV_CC_SetGamma(handle, Gamma);
  if (nRet == MV_OK)
    RCLCPP_INFO(g_node->get_logger(), "Set Gamma: %.3f", Gamma);
  else
    RCLCPP_ERROR(g_node->get_logger(), "Fail to set Gamma");
}

/* ════════════════════════════════════════════════════════════════════════════
 * 信号处理（Ctrl+C）
 * ════════════════════════════════════════════════════════════════════════════ */
static void SignalHandler(int sig)
{
  if (sig == SIGINT) {
    fprintf(stderr, "\nReceived Ctrl+C, exiting...\n");
    exit_flag = true;
  }
}

static void SetupSignalHandler()
{
  struct sigaction sa;
  sa.sa_handler = SignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
}

/* ════════════════════════════════════════════════════════════════════════════
 * WorkThread  —  抓图 & 发布线程
 * ════════════════════════════════════════════════════════════════════════════ */
static void *WorkThread(void *pUser)
{
  int nRet = MV_OK;

  MVCC_INTVALUE stParam;
  memset(&stParam, 0, sizeof(MVCC_INTVALUE));
  nRet = MV_CC_GetIntValue(pUser, "PayloadSize", &stParam);
  if (nRet != MV_OK) {
    printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
    return nullptr;
  }

  MV_FRAME_OUT_INFO_EX      stImageInfo   = {0};
  MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};

  size_t buf_size = static_cast<size_t>(stParam.nCurValue) * 3;
  unsigned char *pData       = static_cast<unsigned char *>(malloc(buf_size));
  unsigned char *pDataForBGR = static_cast<unsigned char *>(malloc(buf_size));

  if (!pData || !pDataForBGR) {
    printf("Memory allocation failed!\n");
    free(pData);
    free(pDataForBGR);
    return nullptr;
  }

  while (!exit_flag && rclcpp::ok()) {
    nRet = MV_CC_GetOneFrameTimeout(pUser, pData, buf_size, &stImageInfo, 1000);
    if (nRet != MV_OK)
      continue;
    printf("Got frame: %d x %d, PixelFormat=0x%x, PayloadSize=%d\n",
           stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.enPixelType, stImageInfo.nFrameLen);

    /* ── 确定时间戳 ──────────────────────────────────────────────────────── */
    rclcpp::Time rcv_time;
    if (trigger_enable && pointt && pointt != MAP_FAILED && pointt->low != 0) {
      /*
       * pointt->low 是 STM32 触发时刻的 PC 时钟纳秒数（int64_t）。
       * rclcpp::Time(int64_t nanoseconds) 直接接受纳秒，无需转换。
       *
       * 对应 ROS1 原版：
       *   double time_pc = b / 1e9;
       *   rcv_time = ros::Time(time_pc);   // ros::Time(double) 接受秒
       */
      rcv_time = rclcpp::Time(pointt->low);
    } else {
      rcv_time = g_node->get_clock()->now();
    }

    /* ── 像素格式转换（转 RGB8） ─────────────────────────────────────────── */
    stConvertParam.nWidth        = stImageInfo.nWidth;
    stConvertParam.nHeight       = stImageInfo.nHeight;
    stConvertParam.pSrcData      = pData;
    stConvertParam.nSrcDataLen   = buf_size;
    stConvertParam.enSrcPixelType = stImageInfo.enPixelType;
    stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
    stConvertParam.pDstBuffer    = pDataForBGR;
    stConvertParam.nDstBufferSize = buf_size;

    nRet = MV_CC_ConvertPixelType(pUser, &stConvertParam);
    if (nRet != MV_OK) {
      printf("MV_CC_ConvertPixelType failed! nRet [%x], skipping frame\n", nRet);
      continue;
    }

    /* ── 构造 cv::Mat 并缩放 ─────────────────────────────────────────────── */
    cv::Mat srcImage(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pDataForBGR);

    if (image_scale > 0.0f && image_scale != 1.0f) {
      cv::resize(srcImage, srcImage,
                 cv::Size(static_cast<int>(srcImage.cols * image_scale),
                          static_cast<int>(srcImage.rows * image_scale)),
                 0, 0, cv::INTER_LINEAR);
    }

    /* ── 发布 ROS2 图像消息 ──────────────────────────────────────────────── */
    // cv_bridge::CvImage::toImageMsg() 返回 sensor_msgs::msg::Image::SharedPtr
    auto msg = cv_bridge::CvImage(std_msgs::msg::Header(), "rgb8", srcImage).toImageMsg();
    msg->header.stamp    = rcv_time;
    msg->header.frame_id = "camera";
    pub.publish(*msg);
  }

  free(pData);
  free(pDataForBGR);
  return nullptr;
}

/* ════════════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  g_node = rclcpp::Node::make_shared("mvs_trigger");

  if (argc < 2) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "Usage: mvs_trigger <config_yaml_path>");
    return -1;
  }
  const std::string params_file = argv[1];

  /* ── 读取配置文件基础字段 ─────────────────────────────────────────────── */
  cv::FileStorage Params(params_file, cv::FileStorage::READ);
  if (!Params.isOpened()) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "Failed to open settings file: %s", params_file.c_str());
    return -1;
  }
  trigger_enable = static_cast<int>(Params["TriggerEnable"]);
  std::string expect_serial = static_cast<std::string>(Params["SerialNumber"]);
  std::string pub_topic     = static_cast<std::string>(Params["TopicName"]);
  int pixel_fmt_idx         = static_cast<int>(Params["PixelFormat"]);

  /* ── 创建 image_transport 发布器 ─────────────────────────────────────── */
  image_transport::ImageTransport it(g_node);
  pub = it.advertise(pub_topic, 1);
  RCLCPP_INFO(g_node->get_logger(), "Publishing to topic: %s", pub_topic.c_str());

  /* ── 打开共享内存文件（时间戳同步） ──────────────────────────────────── */
  const char *user_name = getlogin();
  if (!user_name) {
    RCLCPP_WARN(g_node->get_logger(),
                "getlogin() failed, falling back to /tmp/timeshare");
    user_name = "tmp";
  }
  std::string shm_path = "/home/" + std::string(user_name) + "/timeshare";
  int fd = open(shm_path.c_str(), O_RDWR);
  if (fd < 0) {
    RCLCPP_WARN(g_node->get_logger(),
                "Cannot open shared memory file %s (errno=%d). "
                "Timestamps will use system clock.",
                shm_path.c_str(), errno);
    pointt = nullptr;
  } else {
    pointt = static_cast<time_stamp *>(
      mmap(nullptr, sizeof(time_stamp), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (pointt == MAP_FAILED) {
      RCLCPP_WARN(g_node->get_logger(),
                  "mmap failed (errno=%d). Timestamps will use system clock.", errno);
      pointt = nullptr;
    } else {
      RCLCPP_INFO(g_node->get_logger(),
                  "Shared memory mapped: %s", shm_path.c_str());
    }
    close(fd);  // mmap 后 fd 可以关闭
  }

  SetupSignalHandler();

  /* ── 枚举相机设备 ────────────────────────────────────────────────────── */
  MV_CC_DEVICE_INFO_LIST stDeviceList;
  memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

  int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "MV_CC_EnumDevices fail! nRet [0x%x]", nRet);
    return -1;
  }
  if (stDeviceList.nDeviceNum == 0) {
    RCLCPP_ERROR(g_node->get_logger(), "No camera device found!");
    return -1;
  }

  RCLCPP_INFO(g_node->get_logger(),
              "Found %d device(s):", stDeviceList.nDeviceNum);
  for (unsigned int i = 0; i < stDeviceList.nDeviceNum; ++i) {
    printf("[device %d]:\n", i);
    PrintDeviceInfo(stDeviceList.pDeviceInfo[i]);
  }

  /* ── 选择目标设备 ────────────────────────────────────────────────────── */
  unsigned int nIndex = 0;

  if (stDeviceList.nDeviceNum > 1) {
    if (expect_serial.empty()) {
      RCLCPP_ERROR(g_node->get_logger(),
                   "Multiple cameras found but SerialNumber not set in config!");
      return -1;
    }
    bool found = false;
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; ++i) {
      MV_CC_DEVICE_INFO *info = stDeviceList.pDeviceInfo[i];
      if (!info) continue;

      std::string sn;
      if (info->nTLayerType == MV_USB_DEVICE)
        sn = reinterpret_cast<char *>(info->SpecialInfo.stUsb3VInfo.chSerialNumber);
      else if (info->nTLayerType == MV_GIGE_DEVICE)
        sn = reinterpret_cast<char *>(info->SpecialInfo.stGigEInfo.chSerialNumber);
      else
        continue;

      if (sn == expect_serial) {
        nIndex = i;
        found  = true;
        break;
      }
    }
    if (!found) {
      RCLCPP_ERROR(g_node->get_logger(),
                   "Cannot find camera with serial number: %s", expect_serial.c_str());
      return -1;
    }
  }

  /* ── 创建句柄 & 打开设备 ─────────────────────────────────────────────── */
  void *handle = nullptr;
  nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "MV_CC_CreateHandle fail! nRet [0x%x]", nRet);
    return -1;
  }

  nRet = MV_CC_OpenDevice(handle);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "MV_CC_OpenDevice fail! nRet [0x%x]", nRet);
    return -1;
  }

  /* ── 关闭自动帧率控制，设置像素格式 ─────────────────────────────────── */
  MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", false);

  nRet = MV_CC_SetEnumValue(handle, "PixelFormat", PIXEL_FORMAT_LIST[pixel_fmt_idx]);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(), "Set PixelFormat fail! nRet [0x%x], PixelFormat=0x%x", nRet, PIXEL_FORMAT_LIST[pixel_fmt_idx]);
    return -1;
  }

  /* ── 应用 YAML 中的其余相机参数 ──────────────────────────────────────── */
  setParams(handle, params_file);

  /* ── 触发模式配置 ────────────────────────────────────────────────────── */
  nRet = MV_CC_SetEnumValue(handle, "TriggerMode", trigger_enable);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(),
                 "MV_CC_SetTriggerMode fail! nRet [0x%x]", nRet);
    return -1;
  }

  if (trigger_enable) {
    nRet = MV_CC_SetEnumValue(handle, "TriggerSource", MV_TRIGGER_SOURCE_LINE0);
    if (nRet != MV_OK) {
      RCLCPP_ERROR(g_node->get_logger(),
                   "MV_CC_SetTriggerSource fail! nRet [0x%x]", nRet);
      return -1;
    }
    RCLCPP_INFO(g_node->get_logger(), "Hardware trigger: LINE0");
  }

  /* ── 开始抓图 ────────────────────────────────────────────────────────── */
  RCLCPP_INFO(g_node->get_logger(), "All params set. Start grabbing...");
  nRet = MV_CC_StartGrabbing(handle);
  if (nRet != MV_OK) {
    RCLCPP_ERROR(g_node->get_logger(), "MV_CC_StartGrabbing fail! nRet [0x%x]", nRet);
    return -1;
  }

  /* ── 启动抓图工作线程 ────────────────────────────────────────────────── */
  pthread_t nThreadID = 0;
  nRet = pthread_create(&nThreadID, nullptr, WorkThread, handle);
  if (nRet != 0) {
    RCLCPP_ERROR(g_node->get_logger(), "pthread_create failed, ret=%d", nRet);
    return -1;
  }

  /* ── 主循环：处理 ROS2 回调 ──────────────────────────────────────────── */
  rclcpp::Rate loop_rate(10);
  while (!exit_flag && rclcpp::ok()) {
    rclcpp::spin_some(g_node);
    loop_rate.sleep();
  }

  /* ── 等待工作线程退出 ────────────────────────────────────────────────── */
  if (nThreadID) {
    pthread_join(nThreadID, nullptr);
    RCLCPP_INFO(g_node->get_logger(), "Worker thread joined.");
  }

  /* ── 停止抓图 & 释放资源 ─────────────────────────────────────────────── */
  MV_CC_StopGrabbing(handle);
  MV_CC_CloseDevice(handle);
  MV_CC_DestroyHandle(handle);

  if (pointt && pointt != MAP_FAILED)
    munmap(pointt, sizeof(time_stamp));

  rclcpp::shutdown();
  return 0;
}
