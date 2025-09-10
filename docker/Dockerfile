# FAST-LIVO2 Development Docker Image
# Based on ROS Humble (Ubuntu 22.04)

FROM osrf/ros:humble-desktop

# Accept build arguments for proxy settings
ARG PROXY_HOST=host.docker.internal
ARG PROXY_PORT=7890

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ROS_DISTRO=humble
ENV LIVO2_WS=/home/developer/livo2_ws

# Set proxy environment variables if provided
ENV https_proxy=http://${PROXY_HOST}:${PROXY_PORT}
ENV http_proxy=$https_proxy
ENV no_proxy=localhost,127.0.0.1,::1,192.168.0.0/16

RUN echo 'Asia/Shanghai' > /etc/timezone && ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime

# Update the apt sources to use Tsinghua University's mirror
RUN <<EOF cat > /etc/apt/sources.list
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-backports main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-security main restricted universe multiverse
EOF

# Add ROS 2 apt repository
RUN curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
RUN mv /etc/apt/sources.list.d/ros2.sources /etc/apt/sources.list.d/ros2.sources.bak && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] https://mirrors.tuna.tsinghua.edu.cn/ros2/ubuntu jammy main" | tee /etc/apt/sources.list.d/ros2.list > /dev/null

# 手动模拟 rosdep init
RUN mkdir -p /etc/ros/rosdep/sources.list.d/ && curl -o /etc/ros/rosdep/sources.list.d/20-default.list -L https://mirrors.tuna.tsinghua.edu.cn/github-raw/ros/rosdistro/master/rosdep/sources.list.d/20-default.list

# set rosdep mirror to Tsinghua University
ENV ROSDISTRO_INDEX_URL=https://mirrors.tuna.tsinghua.edu.cn/rosdistro/index-v4.yaml

# Install system dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    build-essential \
    libeigen3-dev \
    libpcl-dev \
    libopencv-dev \
    ros-${ROS_DISTRO}-sophus \
    ros-${ROS_DISTRO}-image-transport-plugins \
    ros-${ROS_DISTRO}-pcl-ros \
    python3-rosdep \
    python3-pip \
    unzip \
    vim \
    && rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-c"]

# 安装最新版本的 clangd
RUN CLANGD_VERSION=$(curl -s https://api.github.com/repos/clangd/clangd/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/') && \
    curl -L -o /tmp/clangd-linux.zip "https://github.com/clangd/clangd/releases/download/${CLANGD_VERSION}/clangd-linux-${CLANGD_VERSION}.zip" && \
    unzip /tmp/clangd-linux.zip -d /tmp/clangd-linux && \
    cp -r /tmp/clangd-linux/*/{bin,lib} /usr/local && \
    chmod +x /usr/local/bin/clangd && \
    rm -rf /tmp/clangd-linux.zip /tmp/clangd-linux
    
# Install Livox-SDK2
RUN <<EOF
cd /tmp
curl -OL https://github.com/Livox-SDK/Livox-SDK2/archive/refs/heads/master.zip
unzip master.zip
rm master.zip
cd ./Livox-SDK2-master
mkdir build
cd build
cmake .. && make -j
sudo make install
rm -rf /tmp/Livox-SDK2*
EOF

    
# Create a non-root user
RUN useradd -m -s /bin/bash developer && \
    echo "developer:developer" | chpasswd && \
    usermod -aG sudo developer && \
    echo developer ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/developer && \
    chmod 0440 /etc/sudoers.d/developer

# Switch to developer user
USER developer
WORKDIR /home/developer

# Initialize rosdep
RUN rosdep update

RUN mkdir -p ${LIVO2_WS}/src
WORKDIR ${LIVO2_WS}

RUN git clone https://github.com/wsxq2/profile.git ~/.MyProfile && cd ~/.MyProfile && ./deploy.sh ${PROXY_HOST}

# Set up environment
RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash" >> ~/.bashrc && \
    echo "source ${LIVO2_WS}/install/setup.bash" >> ~/.bashrc && \
    echo "export ROS_PACKAGE_PATH=\$ROS_PACKAGE_PATH:${LIVO2_WS}" >> ~/.bashrc && \
    echo "export ROS_WORKSPACE=${LIVO2_WS}" >> ~/.bashrc

# Set working directory
WORKDIR ${LIVO2_WS}

# Default command
CMD ["/bin/bash"]
