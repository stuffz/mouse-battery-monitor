# Build image: compiler, Qt, libudev and the lint tools live here - nothing
# installs on the host. Arch matches the development machine's toolchain, and
# Qt6Test ships inside qt6-base.
FROM archlinux:latest

RUN pacman -Syu --noconfirm --needed \
        base-devel clang cmake cppcheck git qt6-base systemd-libs \
    && rm -rf /var/cache/pacman/pkg

# A named user for the devcontainer to attach as, so files created on the bind
# mount belong to whoever is developing. make container-* passes -u instead.
RUN useradd -m -u 1000 builder

WORKDIR /work
