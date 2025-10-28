FROM alpine:edge

RUN apk add --no-cache xmake git 7zip wget cmake build-base linux-headers

# install project dependencies
RUN apk add --no-cache gdal-dev

WORKDIR /app

ENV XMAKE_ROOT=y
COPY requires.lua ./
RUN mv requires.lua xmake.lua
RUN xmake require -y
RUN mv xmake.lua requires.lua

# copy project header dependencies
COPY lib ./lib/

COPY xmake.lua ./
COPY src ./src/
RUN xmake build

CMD ["xmake", "run"]
