FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04 as devcontainer

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
    iputils-ping \
    dnsutils \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libgeographic-dev \
    libssl-dev \
    ninja-build \
    libqpid-proton-cpp12-dev

# Build and install Vanetza
WORKDIR /tmp/vanetza
COPY vanetza .
RUN mkdir build \
    && cd build \
    && cmake .. \
    && make -j8\
    && make install

# Build your application
WORKDIR /workspace
COPY . .

RUN mkdir build \
    && cd build \
    && cmake .. \
    && make -j8

# Second stage: Create minimal runtime image
FROM ubuntu:22.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        libboost-date-time1.74.0 \
        libboost-program-options1.74.0 \
        libboost-system1.74.0 \
        libcrypto++8 \
        libgeographic-dev \
        libqpid-proton-cpp12 \
    && rm -rf /var/lib/apt/lists/*

# Copy Vanetza and your application from builder
COPY --from=devcontainer /usr/local/lib/libvanetza* /usr/local/lib/
COPY --from=devcontainer /workspace/build/AZ-V2X /usr/local/bin/

EXPOSE 8080

# Update library cache
RUN ldconfig

# Set the entry point to your application
ENTRYPOINT ["/usr/local/bin/AZ-V2X"]