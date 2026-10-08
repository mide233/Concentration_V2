# Reuse the librmcs development image and add the tools required to flash and
# debug the STM32F103 target over ST-Link with OpenOCD (not part of the base
# image).
FROM mide233/librmcs-develop:latest

USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends openocd \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
USER ubuntu
