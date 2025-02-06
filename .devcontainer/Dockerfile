FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="none"

# Optionally install the cmake for vcpkg
COPY ./.devcontainer/reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \         
    build-essential \
    cmake \
    git \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libgeographic-dev \
    libssl-dev \
    ninja-build

# Build and install Vanetza
WORKDIR /tmp/vanetza
COPY vanetza .
RUN mkdir build \
    && cd build \
    && cmake .. \
    && make \
    && make install

# Set up workspace for main project
WORKDIR /workspace

# # Set CMake prefix path to find vanetza
# ENV CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"
