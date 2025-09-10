# FAST-LIVO2 编译与运行——ROS Humble + Docker

> 部分内容参考自 https://github.com/integralrobotics/FAST-LIVO2 中的 README.md 说明。

使用前，需要[安装并配置 Docker]({% post_url 2022-03-16-Docker使用笔记 %}#安装和配置)

完成后，在 WSL 中执行以下命令安装依赖并下载源码：

```bash
cd ./src # 如果没有请自行创建此目录
git clone https://github.com/integralrobotics/FAST-LIVO2
git clone https://github.com/integralrobotics/rpg_vikit
git clone https://github.com/Livox-SDK/livox_ros_driver2
```

然后从 [fast-livo2-dataset - OneDrive](https://connecthkuhk-my.sharepoint.com/:f:/g/personal/zhengcr_connect_hku_hk/ErdFNQtjMxZOorYKDTtK4ugBkogXfq1OfDm90GECouuIQA?e=KngY9Z) 下载 bag 数据。建议下载最小的 `Retail_Street.bag`。解压并放置到 `data/` 目录。

新建前述文件并放置到正确目录，使目录结构如下：

```
./
├── .devcontainer/
│   └── devcontainer.json
├── .vscode/
├── data/
│   └── Retail_Street.bag
├── docker/
│   ├── .env
│   ├── Dockerfile
│   └── docker-compose.yml
├── src/
│   ├── FAST-LIVO2/
│   ├── livox_ros_driver2/
│   └── rpg_vikit/
└── README.md
```

根据实际情况调整下 `.env` 文件中的内容。其中有三个变量，均要正确配置：

- `PROXY_HOST`：当前设置的是你的主机。这通常是正确的，因为容器一般运行在本地主机上。但你需要确保你开启了 clash 之类的 fq 工具，且需要启用“Allow LAN”相关设置。目前支持的是 HTTP 代理。
- `PROXY_PORT`：当前设置是 7890。这是 clash 的默认端口。
- `DISPLAY`：当前设置的是你的主机。这要求你在主机上安装 X11 服务器，例如 [vcxsrv](https://sourceforge.net/projects/vcxsrv/)。如果没有正确设置此变量，会导致你无法启动 RVIZ 等 GUI 工具。

此外 `devcontainer.json` 中也需要配置代理变量，否则 vscode 拓展可能无法下载。该变量当前为：

```
"http.proxy": "http://host.docker.internal:7890",
```

如前所述，你需要根据自己的实际情况修改。

然后在 WSL 中使用`code .`命令运行 VS Code，然后点击左下角，选择“Reopen folder in container”，等待一段时间后即可自动完成环境搭建。完成后打开 VS Code 中的 Terminal（这里打开的 Teminal 就不再是 WSL 了，而是在 Container 中，即 ROS2 Humble 的环境中），先进行编译操作和 source：

```bash
cd src/livox_ros_driver2
./build.sh humble
cd -
source ./install/setup.bash
```

这里由于 livox_ros_driver2 的设计问题，编译时建议这样编译，否则可能编译不通过。

运行前需要转换 bag 格式，将原本的 ROS1 格式转换为 ROS2 格式：

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
+     type: livox_ros_driver2/msg/CustomMsg
      type_description_hash: RIHS01_94041b4794f52c1d81def2989107fc898a62dacb7a39d5dbe80d4b55e538bf6d
```

编译完成后在不同的 Terminal 中分别运行：

```bash
ros2 launch fast_livo mapping_avia.launch.py use_rviz:=True
ros2 bag play -p ./data/Retail_Street  # 启动后处于暂停状态，前者准备就绪后即可使用空格键开始 play
```
