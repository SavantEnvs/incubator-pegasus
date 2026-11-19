# syntax=docker/dockerfile:1.7
# Commit image for apache/incubator-pegasus (savantenvs integration).
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Libraries upstream's thirdparty/ builds from source, taken from Debian where the
# version matches what the fuzzed code needs (fmt 10.1.1 = upstream's pin).
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libfmt-dev libabsl-dev rapidjson-dev librocksdb-dev libboost-dev libboost-system-dev \
        libboost-filesystem-dev libssl-dev nlohmann-json3-dev libgtest-dev \
        cmake bison flex curl ca-certificates patch \
 && rm -rf /var/lib/apt/lists/*

# Thrift 0.9.3 — upstream's pinned version (thirdparty/CMakeLists.txt, same URL_MD5) with
# upstream's thirdparty/fix_thrift_build_and_link_errors.patch. Debian's 0.19 is
# API-incompatible with src/common/serialization_helper/thrift_helper.h.
ARG THRIFT_VER=0.9.3
ARG THRIFT_SHA256=b0740a070ac09adde04d43e852ce4c320564a292f26521c46b78e0641564969e
COPY --chown=0:0 thirdparty/fix_thrift_build_and_link_errors.patch /tmp/thrift.patch
RUN d=/opt/toolchains/deps/thrift-${THRIFT_VER} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://archive.apache.org/dist/thrift/${THRIFT_VER}/thrift-${THRIFT_VER}.tar.gz \
 && echo "${THRIFT_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && (cd "$d/src" && patch -p1 < /tmp/thrift.patch) \
 && cd /tmp/dep-build \
 && cmake "$d/src" -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DWITH_JAVA=OFF -DWITH_PYTHON=OFF -DWITH_C_GLIB=OFF -DWITH_CPP=ON -DBUILD_TESTING=OFF \
      -DBUILD_EXAMPLES=OFF -DWITH_QT5=OFF -DWITH_QT4=OFF -DWITH_OPENSSL=OFF -DBUILD_COMPILER=ON \
      -DBUILD_TUTORIALS=OFF -DWITH_LIBEVENT=OFF -DWITH_ZLIB=OFF -DWITH_STDTHREADS=ON \
      -DWITH_SHARED_LIB=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_CXX_STANDARD=14 \
      -DCMAKE_C_FLAGS="-O2 -g -gdwarf-3" -DCMAKE_CXX_FLAGS="-O2 -g -gdwarf-3" \
      -DCMAKE_INSTALL_PREFIX="$d" \
 && make -j"$(nproc)" install \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz /tmp/thrift.patch && chmod -R a+rX "$d"

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
