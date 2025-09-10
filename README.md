# FAST-LIVO2 编译与运行——ROS Humble + Docker

> 这部分主要参考自 https://github.com/integralrobotics/FAST-LIVO2 中的 README.md 说明，但细节上有所优化。

执行以下命令安装依赖并下载源码：

```bash
cd ./src # 如果没有请自行创建此目录
git clone https://github.com/integralrobotics/FAST-LIVO2
git clone https://github.com/integralrobotics/rpg_vikit
git clone https://github.com/Livox-SDK/livox_ros_driver2
```

然后从 [fast-livo2-dataset - OneDrive](https://connecthkuhk-my.sharepoint.com/personal/zhengcr_connect_hku_hk/_layouts/15/onedrive.aspx?id=%2Fpersonal%2Fzhengcr%5Fconnect%5Fhku%5Fhk%2FDocuments%2Ffast%2Dlivo2%2Ddataset&ga=1) 下载 bag 数据。建议下载最小的 Retail_Street.bag。解压并放置到 data/ 目录。

此时目录结构如下：

```
./
├── .devcontainer/
│   └── devcontainer.json
├── .vscode/
├── build/
├── data/
├── docker/
│   ├── .dockerignore
│   ├── .env
│   ├── Dockerfile
│   └── docker-compose.yml
├── install/
├── log/
├── src/
│   ├── FAST-LIVO2/
│   ├── livox_ros_driver2/
│   └── rpg_vikit/
└── README.md
```

编译时执行以下命令：

```bash
cd src/livox_ros_driver2
./build.sh humble
```

这里由于 livox_ros_driver2 的设计问题，编译时建议这样编译，否则可能编译不通过。

运行前需要转换 bag 格式，将原本的 ROS1 格式转换为 ROS2 格式：

```bash
pip install rosbags
rosbags-convert --src Retail_Street.bag --dst Retail_Street
```

转换完成后需要修改消息类型，在 `Retail_Street/metadata.yaml` 中做出以下修改：

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
ros2 bag play -p Retail_Street  # 启动后处于暂停状态，前者准备就绪后即可使用空格键开始 play
```