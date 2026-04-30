FROM randomdude/gcc-cross-x86_64-elf:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
        nasm \
        make \
        xorriso \
        mtools \
        e2fsprogs \
        qemu-system-x86 \
        ovmf \
        gdb \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
