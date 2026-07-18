FROM gcr.io/oss-fuzz-base/base-builder:latest

RUN apt-get update && apt-get install -y cmake

COPY . $SRC/folden
WORKDIR $SRC/folden

COPY .clusterfuzzlite/build.sh $SRC/build.sh
