# Official Switch toolchain; project-private build inputs are mounted at runtime.
FROM devkitpro/devkita64:20260219
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cmake ninja-build python3 git ca-certificates && \
    rm -rf /var/lib/apt/lists/*
ENV DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64
