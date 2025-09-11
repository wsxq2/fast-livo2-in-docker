# FAST-LIVO2 Development Docker Image
# Based on ROS Noetic (Ubuntu 20.04)

FROM osrf/ros:noetic-desktop

# Accept build arguments for proxy settings
ARG PROXY_HOST=host.docker.internal
ARG PROXY_PORT=7890

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ROS_DISTRO=noetic
ENV CATKIN_WS=/home/developer/workspace

# Set proxy environment variables if provided
ENV https_proxy=http://${PROXY_HOST}:${PROXY_PORT}
ENV http_proxy=$https_proxy
ENV no_proxy=localhost,127.0.0.1,::1,192.168.0.0/16

RUN echo 'Asia/Shanghai' > /etc/timezone && ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime

# Update the apt sources to use Tsinghua University's mirror
RUN <<EOF cat > /etc/apt/sources.list
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-backports main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-security main restricted universe multiverse
EOF
# Add ROS 1 apt repository
RUN rm -rf /etc/apt/sources.list.d/ros1-snapshots.list && echo "deb https://mirrors.tuna.tsinghua.edu.cn/ros/ubuntu/ focal main" | tee /etc/apt/sources.list.d/ros-latest.list > /dev/null && apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654

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
    python3-catkin-tools \
    python3-rosdep \
    python3-rosinstall \
    python3-rosinstall-generator \
    python3-wstool \
    unzip \
    wget \
    vim \
    ros-${ROS_DISTRO}-image-transport-plugins \
    ros-${ROS_DISTRO}-pcl-ros \
    && rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-c"]

# 安装最新版本的 clangd
RUN CLANGD_VERSION=$(curl -s https://api.github.com/repos/clangd/clangd/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/') && \
    curl -L -o /tmp/clangd-linux.zip "https://github.com/clangd/clangd/releases/download/${CLANGD_VERSION}/clangd-linux-${CLANGD_VERSION}.zip" && \
    unzip /tmp/clangd-linux.zip -d /tmp/clangd-linux && \
    cp -r /tmp/clangd-linux/*/{bin,lib} /usr/local && \
    chmod +x /usr/local/bin/clangd && \
    rm -rf /tmp/clangd-linux.zip /tmp/clangd-linux
    
# Create a non-root user
ARG USER_UID=1000
ARG USER_GID=${USER_UID}
RUN groupadd --gid ${USER_GID} developer && \
    useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash developer && \ 
    echo "developer:developer" | chpasswd && \
    usermod -aG sudo developer && \
    echo developer ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/developer && \
    chmod 0440 /etc/sudoers.d/developer

# Switch to developer user
USER developer
WORKDIR /home/developer

# Initialize rosdep
RUN rosdep update

# Create catkin workspace
RUN mkdir -p ${CATKIN_WS}/src
WORKDIR ${CATKIN_WS}

# Install Sophus (specific version for FAST-LIVO2)
RUN cd /tmp && \
    curl -OL https://github.com/strasdat/Sophus/archive/a621ff2e56c56c839a6c40418d42c3c254424b5c.zip && \
    unzip a621ff2e56c56c839a6c40418d42c3c254424b5c.zip && \
    cd Sophus-* && \
    sed -i -E \
    -e 's/unit_complex_\.real\(\) = ([0-9.]+);/unit_complex_.real(\1);/' \
    -e 's/unit_complex_\.imag\(\) = ([0-9.]+);/unit_complex_.imag(\1);/' \
    ./sophus/so2.cpp && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf /tmp/Sophus

RUN git clone https://github.com/wsxq2/profile.git ~/.MyProfile && cd ~/.MyProfile && ./deploy.sh ${PROXY_HOST}

# Set up environment
RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash" >> ~/.bashrc && \
    echo "source ${CATKIN_WS}/devel/setup.bash" >> ~/.bashrc && \
    echo "export ROS_PACKAGE_PATH=\$ROS_PACKAGE_PATH:${CATKIN_WS}" >> ~/.bashrc && \
    echo "export ROS_WORKSPACE=${CATKIN_WS}" >> ~/.bashrc && \
    echo "export DISABLE_ROS1_EOL_WARNINGS=1" >> ~/.bashrc

# Set working directory
WORKDIR ${CATKIN_WS}

# Default command
CMD ["/bin/bash"]
