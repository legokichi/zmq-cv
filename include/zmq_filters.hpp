#ifndef ZMQ_FILTER_HPP
#define ZMQ_FILTER_HPP

#include <string>
#include <vector>
#include <utility>
#include <tbb/pipeline.h>
#include <opencv2/core.hpp>
#include <zmq_reader.hpp>

namespace zmq {

class Source: public tbb::filter {
public:
  zmq::VideoCapture reader;
  Source(const string& zmq_endpoint, const int zmq_socktype, const int inactivity_timeout)
  : filter(tbb::filter::mode::serial_in_order) {
    reader = zmq::VideoCapture{zmq_endpoint, zmq_socktype, inactivity_timeout};
    printf("finaly: %dx%d,%f\n", reader.width, reader.height, reader.fps);
  };
  void* operator()(void*){
    if(reader.isOpened()){
      auto mat = new cv::Mat{};
      reader >> mat;
      auto timestamp = reader.timestamp;
      return static_cast<void*>(mat);
    }else{
      std::cout << "finished" << std::endl;
      return nullptr;
    }
  };
};

}
#endif // ZMQ_FILTER_HPP