# 在 Docker 中开发 FAST-LIVO2

[livox_ros_driver2]: https://github.com/Livox-SDK/livox_ros_driver2
[livox_ros2_driver]: https://github.com/wsxq2/livox_ros2_driver.git
[FAST-Calib-ROS2]: https://github.com/ichangjian/FAST-Calib-ROS2.git
[LIV_handhold]: https://github.com/xuankuzcr/LIV_handhold

本仓库主要分为两部分：算法和传感器驱动。

算法主要由两个包组成：
- FAST-LIVO2：核心算法。基于 <https://github.com/integralrobotics/FAST-LIVO2> 的修改版本，主要修改内容是修正了相机内外参配置，以及将对于 [livox_ros_driver2] 的依赖变成了对于 livox_interfaces 的依赖，这是由于我们使用的激光雷达是官方使用的 Avia，但该激光雷达的 ROS2 驱动为 [livox_ros2_driver] 而非 [livox_ros_driver2]，而在 [livox_ros2_driver] 中，拆分为了三个子包：
  - livox_ros2_driver：驱动本身，支持 Avia 等激光雷达
  - livox_interfaces: 消息接口，包括 CustomMsg.msg 等
  - livox_sdk_vendor：辅助安装 Livox SDK
- rpg_vikit：工具包

传感器驱动主要包括两个部分：相机和激光雷达。
- 相机驱动：即 mvs_ros_pkg 包
- 激光雷达驱动：即前述的 [livox_ros2_driver] 包，注意，它改自官方的驱动，主要修改是添加了相机时间戳同步机制及更新了 broadcast code，前者参考了 [LIV_handhold](https://github.com/xuankuzcr/LIV_handhold/tree/main/mvs_ros_driver) 中的相关代码。

此外还有 FAST-LIVO2 的官方标定包（[FAST-Calib-ROS2]）也作为 submodule 被引用，且其配置文件单独提取出来作为一个包（fast-livo2-calib），避免不必要的改动。

需要注意的是，由于本仓库包含 submodule，在一般的 `git clone` 步骤还应执行以下命令下载 submodule：

```bash
git submodule update --init
```

## 关键硬件说明

- 相机
  - 镜头：海康 MVL-HF0628M-6MPE
  - 相机：海康 MV-CU013-A0UC。选择这个是因为官方推荐的 MV-CA013-21UC 已经停产了。海康官方推荐的平替产品是这个。
- 激光雷达：大疆 Livox Avia
- 计算平台：树莓派 4B，4GB 内存。官方使用的[大疆妙算2](https://www.dji.com/cn/manifold-2)也停产了，所以选择使用便宜好用的树莓派。

## 环境配置

本仓库使用的环境包括两个部分：local 和 remote。前者用于跑包，后者用于采包。下面分别说明

### local

- ROS 版本：ROS2 humble。运行于 Docker 容器中
- 操作系统：Docker + Ubuntu 22.04 容器
- IDE: VS Code

使用前，需要[安装并配置 Docker](https://wsxq2.55555.io/blog/2022/03/16/Docker%E4%BD%BF%E7%94%A8%E7%AC%94%E8%AE%B0/#%E5%AE%89%E8%A3%85%E5%92%8C%E9%85%8D%E7%BD%AE)

本仓库中决定了 Docker 容器环境的文件主要是以下文件：

```
./
├── .devcontainer/
│   └── devcontainer.json
├── docker/ # docker 相关文件
│   ├── .env
│   ├── Dockerfile
│   └── docker-compose.yml
```

其中，我们需要根据实际情况调整下 `docker/.env` 文件中的内容，如果没有此文件则请手动添加（也可不添加，此时使用默认值），参考内容如下：

```bash
PROXY_HOST=host.docker.internal
PROXY_PORT=7890
USER_UID=1000
DISPLAY=host.docker.internal:0.0
```

其中的变量均要正确配置：

- `PROXY_HOST`：当前设置的是你的主机。这通常是正确的，因为容器一般运行在本地主机上。但你需要确保你开启了 clash 之类的 fq 工具，且需要启用“Allow LAN”相关设置。目前支持的是 HTTP 代理。
- `PROXY_PORT`：当前设置是 7890。这是 clash 的默认端口。
- `DISPLAY`：当前设置的是你的主机。这要求你在主机上安装 X11 服务器，例如 [vcxsrv](https://sourceforge.net/projects/vcxsrv/)。如果没有正确设置此变量，会导致你无法启动 RVIZ 等 GUI 工具。
- `USER_UID`: 务必设置为你的用户的 UID，否则在 docker 中无权限操作当前目录。

然后在 WSL 中使用`code .`命令运行 VS Code，然后点击左下角，选择“Reopen folder in container”，等待一段时间后即可自动完成环境搭建。完成后打开 VS Code 中的 Terminal（这里打开的 Teminal 就不再是 WSL 了，而是在 Container 中，即 ROS2 Humble 的环境中），即可进行编译和运行操作等。

**VSCode Dev Container 简要说明**：vscode 的 dev container 功能让我们可以在 vscode 中快速构建 Docker 容器，搭建开发环境，并在其中进行开发和测试。关于此功能的更多信息，可参考 [Dev Containers - Visual Studio Marketplace](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) 中提到的相关文档。关于使用此功能搭建开发环境的更多信息，可参考我的个人博客 [Docker + ROS2 开发环境搭建指南](https://wsxq2.55555.io/blog/2025/07/29/docker-ros2-%E5%BC%80%E5%8F%91%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA%E6%8C%87%E5%8D%97/) 及 [使用 VSCode 打造多平台多语言通用的 IDE](https://wsxq2.55555.io/blog/2025/05/26/%E4%BD%BF%E7%94%A8vscode%E6%89%93%E9%80%A0%E5%A4%9A%E5%B9%B3%E5%8F%B0%E5%A4%9A%E8%AF%AD%E8%A8%80%E7%9A%84IDE/#github-pages-%E5%8D%9A%E5%AE%A2)


### remote

采包的计算平台可以是多样的，你可以使用 jetson、树莓派、妙算2、ARM 开发板等，只要你安装 ros2 humble 的环境，且可连接到前述传感器即可。

我们使用的计算平台是树莓派 4b 4GB 内存 + 8GB swap。

## 采包

采包通常在设备上的计算平台中进行，比如官方使用的 [LIV_handhold] 中的妙算平台，或者我们自行搭建使用的树莓派 4B，后续以树莓派 4B 为例。在进一步执行前，需要在树莓派中 clone 本仓库，并安装相机 SDK（TODO：补充相关链接和文档）。

注：激光雷达的 SDK 不需要手动安装，livox_sdk_vendor 包提供了自动安装的能力。

由于我们在采包时不需要跑 FAST-LIVO2 算法，所以也不需要编译相关包。因此编译命令如下所示：

```sh
colcon build --packages-up-to livox_ros2_driver # 激光雷达驱动
colcon build --packages-up-to mvs_ros_pkg # 相机驱动
```

编译完成后即可运行，运行命令如下：

```sh
. install/setup.bash
ros2 launch livox_ros2_driver livox_lidar_msg_launch.py # 激光雷达驱动
ros2 launch mvs_ros_pkg mvs_camera_trigger.launch.py # 相机驱动
```

采包直接使用 `ros2 bag record` 命令：

```sh
cd data/
ros2 bag record /livox/lidar /livox/imu /left_camera/image
```

注意这里采集到的包是在树莓派上，需要通过 scp 等命令将其传输到本地 PC 上，且同样放置到 data/ 目录中。

## 官方提供的测试包

如果自己没有条件采包，可以从 [fast-livo2-dataset - OneDrive](https://connecthkuhk-my.sharepoint.com/:f:/g/personal/zhengcr_connect_hku_hk/ErdFNQtjMxZOorYKDTtK4ugBkogXfq1OfDm90GECouuIQA?e=KngY9Z) （此链接似乎失效了，可以尝试 Issue 下网友提供的[百度网盘链接](https://pan.baidu.com/s/13W2VAEBCVJi7PF210r_nSA?pwd=wxit)） 下载 bag 数据。建议下载最小的 `Retail_Street.bag`。解压并放置到 `data/` 目录。

在 ros2 humble 中使用前需要转换 bag 格式，将原本的 ROS1 格式转换为 ROS2 格式：

```bash
pip install rosbags
cd ./data/
rosbags-convert --src Retail_Street.bag --dst Retail_Street
```

转换完成后需要修改消息类型，在 `data/Retail_Street/metadata.yaml` 中做出以下修改：

```diff
rosbag2_bagfile_information:
  compression_format: ''
  compression_mode: ''
  custom_data: {}
  duration:
    nanoseconds: 135470252209
  files:
  - duration:
      nanoseconds: 135470252209
    message_count: 30157
    path: Retail_Street.db3
    ..............
    topic_metadata:
      name: /livox/lidar
      offered_qos_profiles: ''
      serialization_format: cdr
-     type: livox_ros_driver/msg/CustomMsg
+     type: livox_interfaces/msg/CustomMsg
      type_description_hash: RIHS01_94041b4794f52c1d81def2989107fc898a62dacb7a39d5dbe80d4b55e538bf6d
```

## 跑包

执行以下命令进行编译：

```bash
colcon build --packages-up-to fast_livo
```

编译完成后在不同的 Terminal 中分别运行（记得 `source ./install/setup.bash`）：

```bash
ros2 launch fast_livo mapping_avia.launch.py use_rviz:=True
ros2 bag play -p ./data/Retail_Street  # 启动后处于暂停状态，前者准备就绪后即可使用空格键开始 play
```
