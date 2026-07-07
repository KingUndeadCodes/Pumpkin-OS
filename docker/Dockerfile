# docker build docker -t pumpkin-os-buildenv
# docker run --rm -it -v "$(pwd)":/root/env pumpkin-os-buildenv

FROM randomdude/gcc-cross-x86_64-elf

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    nasm \
    mtools \
    dosfstools \
    qemu-system-x86 \
    && rm -rf /var/lib/apt/lists/*

VOLUME /root/env
WORKDIR /root/env
