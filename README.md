# FAST-LIVO2 编译与运行——ROS Noetic + Docker

使用前，需要[安装并配置 Docker](https://wsxq2.55555.io/blog/2022/03/16/Docker%E4%BD%BF%E7%94%A8%E7%AC%94%E8%AE%B0/#%E5%AE%89%E8%A3%85%E5%92%8C%E9%85%8D%E7%BD%AE)

>  由于 Windows 中使用 docker 的推荐方式是在 WSL2 中使用，所以后面假设你使用的是 WSL2。Linux 则不存在 WSL，直接在 Bash 中执行相关命令即可。

Docker 安装并配置完成后，需要从 [fast-livo2-dataset - OneDrive](https://connecthkuhk-my.sharepoint.com/:f:/g/personal/zhengcr_connect_hku_hk/ErdFNQtjMxZOorYKDTtK4ugBkogXfq1OfDm90GECouuIQA?e=KngY9Z) 下载 bag 数据。建议下载最小的 `Retail_Street.bag`。解压并放置到 `data/` 目录。

本项目通过 Git submodule 固定 FAST-LIVO2 和 rpg_vikit 的源码版本。在项目根目录初始化源码：

```bash
git submodule update --init --recursive
```

首次克隆本项目时，也可以使用 `git clone --recurse-submodules <本项目仓库地址>`。拉取主仓库更新后，再执行上述初始化命令，使子模块版本与主仓库记录一致。

子模块中的本地修改不会随主仓库提交一起保存。若要共享这些修改，需要先在子模块中提交并推送到有写权限的仓库（例如自己的 fork），按需更新 `.gitmodules` 中的仓库地址，再在主仓库提交新的子模块版本引用。

此时检查下目录结构，确保目录结构如下：

```
.
├── .devcontainer/
│   └── devcontainer.json
├── .vscode/
├── data/ # bag 数据文件
│   └── Retail_Street.bag
├── docker/ # docker 相关文件
│   ├── .env
│   ├── Dockerfile
│   └── docker-compose.yml
├── src/ # FAST-LIVO2 源码文件
│   ├── FAST-LIVO2/
│   ├── rpg_vikit/
└── README.md
```

根据实际情况调整下 `docker/.env` 文件中的内容，如果没有此文件则请手动添加，参考内容如下：

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

此外 `devcontainer.json` 中也需要配置代理变量，否则 vscode 拓展可能无法下载。该变量当前为：

```
"http.proxy": "http://host.docker.internal:7890",
```

如前所述，你需要根据自己的实际情况修改。

然后在 WSL 中使用`code .`命令运行 VS Code，然后点击左下角，选择“Reopen folder in container”，等待一段时间后即可自动完成环境搭建。完成后打开 VS Code 中的 Terminal，先进行编译操作和 source：

```bash
catkin_make
source ./devel/setup.bash
```

运行自己的设备或采集的数据前，官方提供的配置也需要根据实际标定结果修改，不能直接套用示例值：

- [`avia.yaml`](src/FAST-LIVO2/config/avia.yaml)：根据标定结果修改 `extrin_calib` 中的外参（`extrinsic_T`、`extrinsic_R`、`Rcl`、`Pcl`），并根据时间同步结果调整 `time_offset` 中的相应参数；话题名称和雷达类型等也需要与实际设备及 bag 一致。
- [`camera_pinhole.yaml`](src/FAST-LIVO2/config/camera_pinhole.yaml)：根据相机标定结果修改焦距 `cam_fx`、`cam_fy`、主点 `cam_cx`、`cam_cy` 和畸变系数 `cam_d0` 至 `cam_d3`，并核对图像尺寸 `cam_width`、`cam_height` 及缩放设置 `scale`。

填写参数时需遵循 FAST-LIVO2 对坐标系、变换方向及参数单位的约定。上述配置位于 FAST-LIVO2 子模块内，共享修改时需按前述子模块流程提交。

然后执行以下命令（打开两个 Teminal 分别执行）：

```bash
roslaunch fast_livo mapping_avia.launch
rosbag play data/Retail_Street.bag
```

## 采包

在树莓派上执行以下命令：

```bash
source <(wget -qO- http://fishros.com/install) # 一键安装 ros1 noetic

# clone LIV_handhold
mkdir -p catkin_ws/src
pushd catkin_ws/src
git clone https://github.com/xuankuzcr/LIV_handhold.git

touch LIV_handhold/livox_ros_driver2/CATKIN_IGNORE

# 安装 livox sdk
curl -OL https://github.com/Livox-SDK/Livox-SDK/archive/refs/heads/master.zip
unzip master.zip
cd Livox-SDK-master/build
cmake ..
make
sudo make install
popd

# 安装依赖
sudo apt install --no-install-recommends -y g++ gdb ros-noetic-pcl-ros ros-noetic-rviz ros-noetic-image-transport ros-noetic-cv-bridge

catkin_make -DCMAKE_BUILD_TYPE=Debug

# 准备工作以连接激光雷达
sudo ip addr add 192.168.1.50/24 dev eth0
vim livox_lidar_config.json

# 启动驱动
roslaunch livox_ros_driver livox_lidar_msg.launch
roslaunch mvs_ros_driver mvs_camera_trigger.launch

```
