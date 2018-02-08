#!/bin/bash
set -euvx

install_sodium()
{
  mkdir -p /opt
  pushd /opt
  wget https://github.com/jedisct1/libsodium/releases/download/1.0.15/libsodium-1.0.15.tar.gz
  tar -xf libsodium-1.0.15.tar.gz
  rm -f libsodium-1.0.15.tar.gz
  pushd /opt/libsodium-1.0.15
  ./autogen.sh
  ./configure
  make check
  make install
  ldconfig
  popd
  popd
}

install_zmqpp()
{
  mkdir -p /opt
  pushd /opt
  wget https://github.com/zeromq/zmqpp/archive/4.1.2.zip
  unzip -o 4.1.2.zip
  rm -f 4.1.2.zip
  pushd /opt/zmqpp-4.1.2
  make -j
  make check
  make install
  ldconfig
  make installcheck
  popd
  popd
}

install_opencv()
{
  mkdir -p /opt
  pushd /opt
  wget https://github.com/Itseez/opencv/archive/3.4.0.zip -O opencv-3.4.0.zip
  unzip opencv-3.4.0.zip
  rm -f opencv-3.4.0.zip
  mkdir -p /opt/opencv-3.4.0/build
  pushd /opt/opencv-3.4.0/build
  cmake \
    -DCMAKE_BUILD_TYPE=RELEASE \
    -DBUILD_DOCS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_WITH_DEBUG_INFO=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_OPENCV_WORLD=OFF \
    -DBUILD_OPENCV_DNN=OFF \
    -DBUILD_opencv_nonfree=ON \
    -DBUILD_OPENCV_PYTHON=OFF \
    -DBUILD_OPENCV_PYTHON2=OFF \
    -DBUILD_OPENCV_PYTHON3=OFF \
    -DBUILD_NEW_PYTHON_SUPPORT=OFF \
    -DWITH_OPENGL=ON \
    -DWITH_OPENCL=ON \
    -DWITH_TBB=ON \
    -DWITH_EIGEN=ON \
    -DWITH_FFMPEG=OFF \
    -DWITH_GSTREAMER=ON \
    -DWITH_V4L=ON \
    -DWITH_CUDA=OFF \
    -DWITH_CUBLAS=OFF \
    -DWITH_NVCUVID=OFF \
    -DWITH_CUFFT=OFF \
    -DCUDA_FAST_MATH=ON \
    -DWITH_PTHREADS_PF=ON \
    -DWITH_IPP=ON \
    -DENABLE_PRECOMPILED_HEADERS=ON \
    -DENABLE_FAST_MATH=ON \
    -DENABLE_CXX11=ON \
    ..
  #  -DENABLE_CXX11=ON \
  #  -DCXXFLAGS="-std=c++14" \
  #  -DCUDA_NVCC_FLAGS="-std=c++11 --expt-relaxed-constexpr" \
  make -j
  make install
  popd
  popd
}


install_date()
{
  mkdir -p /opt
  pushd /opt
  wget https://github.com/HowardHinnant/date/archive/v2.4.zip
  unzip -o v2.4.zip
  rm -f v2.4.zip
  mkdir -p /opt/date-2.4/build
  pushd /opt/date-2.4/build
  cmake ..
  make -j
  make install
  popd
  popd
}



install_date
install_sodium
install_zmqpp
install_opencv
