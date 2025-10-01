# Dockerfile
FROM ubuntu:24.04

# Prevent interactive prompts during package install
ENV DEBIAN_FRONTEND=noninteractive

# Install packages
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        python3 python3-pip \
        git curl wget vim nano \
        && rm -rf /var/lib/apt/lists/*

# Default working directory inside container
WORKDIR /workspace

# Default shell
CMD ["/bin/bash"]
