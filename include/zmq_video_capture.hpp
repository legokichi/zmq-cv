#ifndef ZMQ_VIDEO_CAPTURE_HPP
#define ZMQ_VIDEO_CAPTURE_HPP

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
  uint32_t frame_index;
private:
  bool finish = false;
  ZMQReader reader;

  int counter = 0;

  cv::Mat mat0;
  frame_timestamp timestamp0;
  uint32_t frame_idx0;

  cv::Mat matN;
  frame_timestamp timestampN;
  uint32_t frame_idxN;
public:
  VideoCapture(const string& zmq_endpoint, const string& zmq_socktype, const int inactivity_timeout)
  : reader{zmq_socktype == "SUB"  ? SOCKTYPE_SUB
         : zmq_socktype == "PULL" ? SOCKTYPE_PULL
         :                          SOCKTYPE_PULL,
      zmq_endpoint,
      static_cast<uint32_t>(inactivity_timeout)} {
    auto opt0 = reader.read(mat0);
    auto opt1 = reader.read(matN);
    if(opt0 == std::nullopt || opt1 == std::nullopt){ throw std::runtime_error{"cannot get first 2 frames"}; }
    auto [_timestamp0, _frame_idx0] = *opt0;
    auto [_timestamp1, _frame_idx1] = *opt1;
    using namespace date;
    std::cerr << _timestamp0 << "," << _frame_idx0 << "," << mat0.size().width << "x" << mat0.size().height << "\n";
    std::cerr << _timestamp1 << "," << _frame_idx1 << "," << matN.size().width << "x" << matN.size().height << "\n";
    auto to_unix_time = [](frame_timestamp timestamp) -> int64_t { return chrono::duration_cast<chrono::milliseconds>(timestamp.time_since_epoch()).count(); };
    auto duration_ms = to_unix_time(_timestamp1) - to_unix_time(_timestamp0);
    fps = 1000 / duration_ms;
    width = matN.size().width;
    height = matN.size().height;
    size = cv::Size{width, height};
    frame_idx0 = _frame_idx0;
    timestamp0 = _timestamp0;
    frame_idxN = _frame_idx1;
    timestampN = _timestamp1;
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
    if(counter == 0){ mat = mat0; timestamp = timestamp0; frame_index = frame_idx0; counter++; return *this; }
    if(finish){ return *this; }
    mat = matN;
    timestamp = timestampN;
    frame_index = frame_idxN;
    counter++;
    if(auto opt = reader.read(mat); opt != std::nullopt){
      auto [_timestamp, _frame_idx] = *opt;
      timestampN = _timestamp;
      frame_idxN = _frame_idx;
    }else{
      std::cout << "zmq video capture finished" << std::endl;
      finish = true;
    }
    return *this;
  }
};

}

#endif // ZMQ_VIDEO_CAPTURE_HPP