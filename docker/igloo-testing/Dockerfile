# The engine's Linux CI base image, replaceable instead of inherited.
#
# History: CI used storminator16/igloo-testing:latest, a bullseye image whose
# provenance nobody in the repo could reconstruct, and bullseye's security
# suite died under it in the v2.3.1 release (three workflow failures before
# the build even compiled). This Dockerfile IS the base image now - bookworm,
# everything the spec suite, examples, editor and release packaging need,
# with igloo + snowhouse built from source exactly as README.md documents.
# Bump DEBIAN to the next stable and rebuild when this one goes EOL.
#
# Build and publish (owner):
#   docker buildx build --platform linux/amd64,linux/arm64 \
#     -t storminator16/igloo-testing:bookworm --push docker/igloo-testing
#
# The multi-platform push is what makes the arm64 release job possible;
# a single-arch image fails the arm64 matrix on pull.

ARG DEBIAN=bookworm
FROM debian:${DEBIAN}

ARG IGLOO_REF=5346accd34da324c12dc51e263072daef879ed7b

# Everything the engine builds against, from README.md's "Prerequisites",
# plus what the release path needs: dpkg-dev (dpkg-shlibdeps derives the
# .deb's Depends:), time (examples.mk and editor/Makefile shell out to
# /usr/bin/time), cmake + git + ca-certificates (the igloo build below),
# pkg-config and zlib1g-dev (the net module links -lz, examples link it
# through base.mk's LIB).
#
# The two symlinks put Lua where base.mk expects it: Debian versions both
# the library and the header directory (/usr/include/lua5.4), but base.mk
# only ever searches -I/usr/local/include and links -llua - which is where
# a from-source Lua install lands, and how the README tells a host to be
# set up. Without the header link, vendor/sol/sol.hpp fails on <lua.h> and
# the editor does not compile.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    dpkg-dev \
    git \
    libglm-dev \
    libgtk-3-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libsdl2-ttf-dev \
    liblua5.4-dev \
    pkg-config \
    time \
    valgrind \
    zlib1g-dev \
    && ln -sf $(find /usr/lib -name 'liblua5.4.so' | head -1) /usr/local/lib/liblua.so \
    && ln -sf /usr/include/lua5.4/*.h /usr/include/lua5.4/lua.hpp /usr/local/include/ \
    && rm -rf /var/lib/apt/lists/*

# Igloo + snowhouse, the test framework, pinned to the master commit this
# image first shipped with, so a rebuild of the image cannot silently change
# the test framework under a release. (master's last commit is from 2019;
# the pin is insurance, not paranoia about upstream churn.) The snowhouse
# submodule step is the README's own recipe: the repo does not carry
# snowhouse by default.
#
# Installed as headers, NOT through igloo's own cmake: its 2019 test targets
# do not survive a bookworm gcc with -Werror, and the framework is
# header-only - what the suite needs is <igloo/igloo_alt.h> resolving to
# these two trees, which is also what the README's cmake install produced.
RUN git clone https://github.com/codewars/igloo.git /tmp/igloo \
    && cd /tmp/igloo \
    && git checkout ${IGLOO_REF} \
    && git submodule add -b headers-only https://github.com/banditcpp/snowhouse snowhouse \
    && git submodule update --init --recursive \
    && cp -r igloo /usr/local/include/ \
    && cp -r snowhouse /usr/local/include/ \
    && rm -rf /tmp/igloo

# Sanity gate: the image is useless if the framework it ships cannot be
# included. Cheaper than discovering that in every CI job forever.
RUN printf '#include <igloo/igloo_alt.h>\nint main() { return 0; }\n' > /tmp/probe.cpp \
    && g++ -std=c++17 /tmp/probe.cpp -o /tmp/probe \
    && rm -f /tmp/probe.cpp /tmp/probe
