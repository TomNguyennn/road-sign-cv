FROM ubuntu:24.04 AS backend-build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build gcc-13 g++-13 make pkg-config git ca-certificates nasm \
    libssl-dev zlib1g-dev uuid-dev libpng-dev libjpeg-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake/ ./cmake/
RUN cmake --preset dependencies && cmake --build --preset dependencies
COPY backend/ ./backend/
COPY config/ ./config/
RUN cmake --preset dev && cmake --build --preset dev --target road-sign-server

FROM backend-build AS backend-tests
RUN cmake --build --preset dev && ctest --preset dev

FROM node:22.23.5-bookworm-slim AS frontend-build
WORKDIR /frontend
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci --ignore-scripts
COPY frontend/ ./
RUN npm run build

FROM ubuntu:24.04 AS backend-runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-dev libtesseract-dev libespeak-ng1 libuuid1 libssl3 zlib1g \
    libpng16-16 libjpeg-turbo8 ca-certificates && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home roadsign
COPY --from=backend-build /workspace/build/native /opt/road-sign
COPY --from=backend-build /workspace/build/dev/backend/road-sign-server /usr/local/bin/road-sign-server
COPY config/policy.json /opt/road-sign/config/policy.json
WORKDIR /opt/road-sign
ENV LD_LIBRARY_PATH=/opt/road-sign/lib POLICY_CONFIG_PATH=/opt/road-sign/config/policy.json
USER 10001:10001
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/road-sign-server"]

FROM caddy:2.10.2-alpine AS web-runtime
COPY config/Caddyfile /etc/caddy/Caddyfile
COPY --from=frontend-build /frontend/dist /srv
EXPOSE 443
