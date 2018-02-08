FROM ubuntu:16.04
#FROM nvidia/cuda:8.0-cudnn6-devel-ubuntu16.04
#FROM nvcr.io/nvidia/cuda:9.0-cudnn7-devel-ubuntu16.04
ENV DEBIAN_FRONTEND "noninteractive"

WORKDIR /opt
ADD script/install_deps.bash /opt
RUN bash /opt/install_deps.bash
ADD script/build_deps.bash /opt
RUN bash /opt/build_deps.bash
WORKDIR /opt/identity-docker
ADD . /opt/identity-docker
RUN mkdir -p /opt/identity-docker/build && \
  cd /opt/identity-docker/build && \
  cmake .. && \
  make -j && \
  chmod +x ../identity.bash
