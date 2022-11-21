FROM debian:bookworm
RUN apt-get update
RUN apt-get install -y libwlroots-dev xwayland meson cmake libcairo2-dev libpango1.0-dev
ADD * ./
ADD examples ./examples
RUN meson build
RUN ninja -C build