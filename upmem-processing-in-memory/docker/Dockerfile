# UPMEM SDK 2023.1.0 with its functional simulator, on x86-64 Ubuntu 20.04.
# The SDK is taken from the public CMU-SAFARI/UPMEM repository.
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
        build-essential git ca-certificates pkg-config \
        python3 python3-pip python3.8-dev \
        libedit-dev libxml2 libelf-dev libnuma-dev \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install six
# Only needed for tools/image_tool.py and scripts/plot.py:
RUN pip3 install pillow numpy matplotlib
RUN git clone --depth 1 https://github.com/CMU-SAFARI/UPMEM.git /tmp/upmem \
    && mv /tmp/upmem/upmem-2023.1.0-Linux-x86_64 /opt/upmem-sdk \
    && rm -rf /tmp/upmem
WORKDIR /work
# "simulator" makes every dpu_alloc() use the simulator instead of real hardware.
CMD ["bash", "-c", "source /opt/upmem-sdk/upmem_env.sh simulator && exec bash"]
