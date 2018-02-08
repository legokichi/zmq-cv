#ifndef ZMQ_VIDEO_CAPTURE_HPP
#define ZMQ_VIDEO_CAPTURE_HPP

#include <sstream>
#include <date/date.h>
#include <zmq_reader.hpp>

namespace zmq {

using std::stringstream;

class VideoCapture {
public:
  double fps;
  cv::Size size;
  int width;
  int height;
  frame_timestamp timestamp;
  string timestamp_string;
  uint32_t frame_index;
private:
  bool finish = false;
  ZMQReader reader;

  int counter = 0;

  cv::Mat mat0;
  frame_timestamp timestamp0;
  uint32_t frame_idx0;

  cv::Mat mat1;
  frame_timestamp timestamp1;
  uint32_t frame_idx1;
public:
  VideoCapture(const string& zmq_endpoint, const string& zmq_socktype, const int inactivity_timeout)
  : reader{zmq_socktype == "SUB"  ? SOCKTYPE_SUB
         : zmq_socktype == "PULL" ? SOCKTYPE_PULL
         :                          SOCKTYPE_PULL,
      zmq_endpoint,
      static_cast<uint32_t>(inactivity_timeout)} {
    auto opt0 = reader.read(mat0);
    auto opt1 = reader.read(mat1);
    if(opt0 == std::nullopt || opt1 == std::nullopt){ throw std::runtime_error{"cannot get first 2 frames"}; }
    auto [_timestamp0, _frame_idx0] = *opt0;
    auto [_timestamp1, _frame_idx1] = *opt1;
    using namespace date;
    std::cerr << _timestamp0 << "," << _frame_idx0 << "," << mat0.size().width << "x" << mat0.size().height << "\n";
    std::cerr << _timestamp1 << "," << _frame_idx1 << "," << mat1.size().width << "x" << mat1.size().height << "\n";
    auto to_unix_time = [](auto timestamp){ return chrono::duration_cast<chrono::milliseconds>(timestamp.time_since_epoch()).count(); };
    auto duration_ms = (to_unix_time(timestamp1) - to_unix_time(timestamp0));
    fps = 1000 / duration_ms;
    width = mat1.size().width;
    height = mat1.size().height;
    size = cv::Size{width, height};
    frame_idx0 = _frame_idx0;
    timestamp0 = _timestamp0;
    frame_idx1 = _frame_idx1;
    timestamp1 = _timestamp1;
  }
  ~VideoCapture(){}
  bool isOpened() const { return !finish; }
  void release(){}
  double get(const int capProperty){
    switch(capProperty){
      case CV_CAP_PROP_FRAME_WIDTH: return width;
      case CV_CAP_PROP_FRAME_HEIGHT: return height;
      case CV_CAP_PROP_FPS: return fps;
      case CV_CAP_PROP_POS_FRAMES: return reader.frame_index;
      case CV_CAP_PROP_FRAME_COUNT: return -1; // is it works ? check opencv video capture
      default:
        std::cerr << "unknown prop " << capProperty << "\n";
        return 0;
    }
  }
  void set(const int capProperty, const double value){
    std::cerr << "set(" << capProperty << "," << value << ")\n";
  }
  VideoCapture& operator>>(cv::Mat &mat){
    using namespace date;
    stringstream strm;
    if(counter == 0){ mat = mat0; timestamp = timestamp0; frame_index = frame_idx0; strm << timestamp; timestamp_string = strm.str(); counter++; return *this; }
    if(counter == 1){ mat = mat1; timestamp = timestamp1; frame_index = frame_idx1; strm << timestamp; timestamp_string = strm.str(); counter++; return *this; }
    if(auto opt = reader.read(mat); opt != std::nullopt){
      auto [_timestamp, _frame_idx] = *opt;
      timestamp = _timestamp;
      frame_index = _frame_idx;
      strm << timestamp;
      timestamp_string = strm.str();
      return *this;
    }
    std::cout << "zmq video capture finished" << std::endl;
    finish = true;
    return *this;
  }
};

}

#endif // ZMQ_VIDEO_CAPTURE_HPP