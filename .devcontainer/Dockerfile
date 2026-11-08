FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        gdb \
        clang-format \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspaces/Car-Rental-System
