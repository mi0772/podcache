FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libc6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -r -s /bin/false podcache

WORKDIR /app
COPY --from=builder /app/build/podcache .
COPY --from=builder /app/README.md .

USER podcache
EXPOSE 8080

CMD ["./podcache"]