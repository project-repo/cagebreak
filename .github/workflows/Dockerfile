FROM archlinux:latest


RUN bash scripts/install-development-environment
RUN meson setup build/ -Dxwayland=true -Dman-pages=true --buildtype=release
RUN ninja -C build/
RUN meson test -C build/
