FROM debian:latest AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    make \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

COPY Makefile /workspace/
COPY src /workspace/src/

RUN make -C /workspace

FROM debian:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
    netbase \
    python3 \
    pipx \
    && rm -rf /var/lib/apt/lists/*

RUN PIPX_BIN_DIR=/usr/local/bin pipx install robotframework

COPY --from=builder /workspace/ft_ping /ft_ping
COPY test/resources/ /resources/
COPY test/test/ /test/

ENTRYPOINT [ "robot", "--outputdir", "output", "test" ]
