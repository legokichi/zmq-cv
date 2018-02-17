#!/bin/bash
set -euvx


initial_update()
{
  apt-get update -y
  apt-get -y \
    -o Dpkg::Options::="--force-confdef" \
    -o Dpkg::Options::="--force-confold" dist-upgrade
}

install_build_tools()
{
  apt-get install -y --no-install-recommends \
    dconf-tools \
    curl wget libcurl4-openssl-dev \
    tar zip unzip zlib1g-dev bzip2 libbz2-dev \
    openssl libssl-dev \
    git apt-transport-https software-properties-common ppa-purge apt-utils ca-certificates \
    build-essential binutils cmake pkg-config libtool autoconf automake autogen
  
  add-apt-repository ppa:ubuntu-toolchain-r/test
  apt-get update -y
  apt-get install -y --no-install-recommends \
    gcc-6 g++-6 gcc-7 g++-7
  #update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 20
  #update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 20
}

install_debug_tools()
{
  apt-get install -y --no-install-recommends \
    vim screen tree htop \
    gdb \
    ffmpeg \
    sudo
}

install_boost_tbb_blas()
{
  apt-get install -y --no-install-recommends \
    libboost-all-dev \
    libtbb2 libtbb-dev \
    libatlas-base-dev libopenblas-base libopenblas-dev \
    libeigen3-dev liblapacke-dev
  apt-get install -y --no-install-recommends \
    libzmq3-dev
}


install_gstreamer()
{
  apt-get install -y \
    gstreamer1.0-tools \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-good1.0-dev \
    libgstreamer-plugins-bad1.0-dev

#    gstreamer1.0-libav-dbg \
#    gstreamer1.0-plugins-base-apps \
#    gstreamer1.0-plugins-base-dbg \
#    gstreamer1.0-plugins-good-dbg \
#    gstreamer1.0-plugins-bad-dbg \
#    gstreamer1.0-plugins-bad-faad \
#    gstreamer1.0-plugins-bad-videoparsers \
#    gstreamer1.0-plugins-ugly-dbg \
#    libgstreamer1.0-0-dbg \
# * gst-inspect-1.0
# * https://gstreamer.freedesktop.org/documentation/plugins.html
# * https://users.atmark-techno.com/node/2021
}

cleanup()
{
  apt-get update -y
  apt-get upgrade -y
  apt-get dist-upgrade -y
  apt-get update -y
  apt-get upgrade -y
  apt-get autoremove -y
  apt-get autoclean -y
  rm -rf /var/lib/apt/lists/*
}


while [ $# -gt 0 ];
do
  case "$1" in
    all)
      initial_update
      install_build_tools
      #install_debug_tools
      install_boost_tbb_blas
      install_gstreamer
      cleanup
      ;;
    update) initial_update ;;
    build) install_build_tools ;;
    debug) install_debug_tools ;;
    boost) install_boost_tbb_blas ;;
    gstreamer) install_gstreamer ;;
    cleanup) cleanup ;;
    *) exit 1 ;;
  esac
  shift
done
