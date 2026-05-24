FROM ghcr.io/userver-framework/ubuntu-22.04-userver:v2.13 AS build

WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j "$(nproc)"

FROM ghcr.io/userver-framework/ubuntu-22.04-userver:v2.13

WORKDIR /app
COPY --from=build /app/build/profi_service /app/profi_service
COPY configs ./configs

EXPOSE 8080

CMD ["/app/profi_service", "--config", "/app/configs/static_config.yaml"]
