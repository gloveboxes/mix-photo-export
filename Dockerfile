FROM alpine:3.23 AS build

RUN apk add --no-cache build-base ca-certificates cmake git libjpeg-turbo-dev

WORKDIR /src
COPY CMakeLists.txt decode_mix.cpp storage.cpp ./
COPY tests/ tests/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j4 \
    && ctest --test-dir build --output-on-failure \
    && strip build/mix-photo-export

FROM build AS test
CMD ["ctest", "--test-dir", "build", "--output-on-failure"]

FROM alpine:3.23 AS runtime
RUN apk add --no-cache libstdc++ libturbojpeg \
    && addgroup -g 10001 mixexport \
    && adduser -D -H -u 10001 -G mixexport -s /sbin/nologin mixexport \
    && mkdir /input /output \
    && chown 10001:10001 /output

COPY --from=build /src/build/mix-photo-export /usr/local/bin/mix-photo-export
COPY --from=build /src/build/_deps/libfpx-src/flashpix.h /usr/share/doc/mix-photo-export/flashpix-notice.h
COPY README.md /usr/share/doc/mix-photo-export/README.md
ENV HOME=/tmp
WORKDIR /output
USER 10001:10001
ENTRYPOINT ["mix-photo-export"]
CMD ["--help"]