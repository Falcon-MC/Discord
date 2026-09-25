FROM ubuntu:24.04 AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake git ninja-build ca-certificates libssl-dev \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt .
COPY src src
RUN cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build \
    && mkdir /out \
    && cp build/FalconDiscord /out/ \
    && find build -name 'libdpp.so*' -exec cp -P {} /out/ \;

FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libssl3t64 zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --no-create-home falcon

WORKDIR /app
COPY --from=build /out .
ENV LD_LIBRARY_PATH=/app
USER falcon
ENTRYPOINT ["./FalconDiscord"]
