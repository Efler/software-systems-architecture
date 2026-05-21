FROM ghcr.io/userver-framework/ubuntu-22.04-userver:v2.13 AS build

WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j "$(nproc)"

FROM ghcr.io/userver-framework/ubuntu-22.04-userver:v2.13

WORKDIR /app
COPY --from=build /app/build/event_producer /app/event_producer
COPY --from=build /app/build/event_consumer /app/event_consumer
COPY configs ./configs

EXPOSE 8080

CMD ["/app/event_producer", "--config", "/app/configs/producer_static_config.yaml"]
