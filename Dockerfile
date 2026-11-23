# Two-stage build: compile with g++, ship a slim runtime image.
FROM debian:bookworm-slim AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
# Single-source COPY keeps the directory names intact, which the
# relative includes (manager_system/..., basic_structures/...) need.
COPY mainApp.cpp ./
COPY manager_system/ ./manager_system/
COPY basic_structures/ ./basic_structures/
RUN g++ -std=c++17 -O2 -Wall -Wextra mainApp.cpp manager_system/*.cpp -o car_rental

FROM debian:bookworm-slim
# UTF-8 console so the Persian UI (`--lang fa`) prints correctly.
ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8
COPY --from=build /src/car_rental /usr/local/bin/car_rental
WORKDIR /workspace
ENTRYPOINT ["car_rental"]
