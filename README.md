# FAST-LIVO2 编译与运行——ROS Noetic + Docker

使用前，需要[安装并配置 Docker](https://wsxq2.55555.io/blog/2022/03/16/Docker%E4%BD%BF%E7%94%A8%E7%AC%94%E8%AE%B0/#%E5%AE%89%E8%A3%85%E5%92%8C%E9%85%8D%E7%BD%AE)

>  由于 Windows 中使用 docker 的推荐方式是在 WSL2 中使用，所以后面假设你使用的是 WSL2。Linux 则不存在 WSL，直接在 Bash 中执行相关命令即可。

Docker 安装并配置完成后，需要从 [fast-livo2-dataset - OneDrive](https://connecthkuhk-my.sharepoint.com/:f:/g/personal/zhengcr_connect_hku_hk/ErdFNQtjMxZOorYKDTtK4ugBkogXfq1OfDm90GECouuIQA?e=KngY9Z) 下载 bag 数据。建议下载最小的 `Retail_Street.bag`。解压并放置到 `data/` 目录。

下载源码：

```bash
cd src
git clone https://github.com/hku-mars/FAST-LIVO2.git
git clone https://github.com/xuankuzcr/rpg_vikit.git 
```

此时检查下目录结构，确保目录结构如下：

```
.
├── .devcontainer/
│   └── devcontainer.json
├── .vscode/
├── data/ # bag 数据文件
│   └── Retail_Street.bag
├── docker/ # docker 相关文件
│   ├── .dockerignore # 可选
│   ├── .env
│   ├── Dockerfile
│   └── docker-compose.yml
├── src/ # FAST-LIVO2 源码文件
│   ├── FAST-LIVO2/
│   ├── rpg_vikit/
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

然后在 WSL 中使用`code .`命令运行 VS Code，然后点击左下角，选择“Reopen folder in container”，等待一段时间后即可自动完成环境搭建。完成后打开 VS Code 中的 Terminal，先进行编译操作和 source：

```bash
catkin_make
source ./devel/setup.bash
```

然后执行以下命令（打开两个 Teminal 分别执行）：

```bash
roslaunch fast_livo mapping_avia.launch
rosbag play data/Retail_Street.bag
```
