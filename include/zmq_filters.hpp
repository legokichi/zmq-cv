#ifndef ZMQ_FILTER_HPP
#define ZMQ_FILTER_HPP

#include <string>
#include <tuple>
#include <tbb/pipeline.h>
#include <opencv2/core.hpp>
#include <zmq_video_capture.hpp>

namespace zmq {

using std::tuple;

class VideoSource: public tbb::filter {
public:
  zmq::VideoCapture reader;
  VideoSource(const string& zmq_endpoint, const string& zmq_socktype, const int inactivity_timeout)
  : filter{tbb::filter::mode::serial_in_order}
  , reader{zmq_endpoint, zmq_socktype, inactivity_timeout} {
    std::cerr << "w,h,fps" << reader.width << reader.height << reader.fps << "\n";
  }
  void* operator()(void*){
    if(reader.isOpened()){
      auto mat = new cv::Mat{};
      reader >> *mat;
      auto timestamp = reader.timestamp;
      auto frame_index = reader.frame_index;
      return static_cast<void*>(new tuple<cv::Mat*, frame_timestamp, uint32_t>{mat, timestamp, frame_index});
    }else{
      std::cout << "zmq source finished\n";
      return nullptr;
    }
  }
};

}
#endif // ZMQ_FILTER_HPP