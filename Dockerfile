FROM ubuntu:16.04
#FROM nvidia/cuda:8.0-cudnn6-devel-ubuntu16.04
#FROM nvcr.io/nvidia/cuda:9.0-cudnn7-devel-ubuntu16.04
ENV DEBIAN_FRONTEND "noninteractive"

WORKDIR /opt
ADD script/install_deps.bash /opt
RUN bash /opt/install_deps.bash all cleanup
#RUN bash /opt/install_deps.bash update build debug boost
#RUN bash /opt/install_deps.bash gstreamer
#RUN bash /opt/install_deps.bash cleanup
ADD script/build_deps.bash /opt
RUN bash /opt/build_deps.bash all cleanup
#RUN bash /opt/build_deps.bash date sodium zmqpp 
#RUN bash /opt/build_deps.bash opencv
#RUN bash /opt/build_deps.bash cleanup
WORKDIR /opt/algorithm
ADD . /opt/algorithm
RUN mkdir -p /opt/algorithm/build && \
  cd /opt/algorithm/build && \
  cmake .. && \
  make -j && \
  chmod +x ../algorithm.bash
