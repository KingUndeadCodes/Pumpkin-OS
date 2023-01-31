# docker build buildenv -t env
# docker run --rm -it -v "$(pwd)":/root/env env

FROM randomdude/gcc-cross-i686-elf

RUN apt-get update 
RUN apt-get upgrade -y
RUN apt-get install -y nasm
RUN apt-get install -y xorriso
RUN apt-get install -y grub-pc-bin
RUN apt-get install -y grub-common

VOLUME /root/env
# WORKDIR .