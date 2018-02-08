#ifndef ZMQ_RECEIVER_HPP
#define ZMQ_RECEIVER_HPP

#include <string>
#include <chrono>
#include <zmqpp/zmqpp.hpp>
#include <opencv2/core.hpp>

namespace zmq {

namespace chrono = std::chrono;
using std::string;
using frame_clock = chrono::system_clock;
using frame_timestamp = chrono::time_point<frame_clock, frame_clock::duration>;

enum struct FrameType { Simple };

struct basic_frame {
  cv::Mat data;
  frame_timestamp timestamp;
  FrameType frame_type = FrameType::Simple;
  string format;
  void set_current_time(void){ this->timestamp = chrono::system_clock::now(); };
};

auto receive_image_frame(zmqpp::socket& sock_, basic_frame& frame_, bool consume_kernel_buffer_=true) -> bool;
auto receive_image_frame(zmqpp::socket& sock_, basic_frame& frame_, bool consume_kernel_buffer_) -> bool {
  using namespace std::chrono;
  // A lossless channel should not consume kernel buffers
  if (sock_.type() == zmqpp::socket_type::pull){
    consume_kernel_buffer_ = false;
  }
  int count = 0;
  try {
    zmqpp::message buf;
    // Consume all frames in the kernel buffer
    while (sock_.receive(buf, /* dont block */ true)) {
      count++;
      string topic, version;
      uint64_t tval;
      int16_t frame_type;
      int32_t rows, cols, type;
      if (sock_.type() == zmqpp::socket_type::subscribe){
        buf >> topic;
      }
      buf >> version
          >> tval
          >> frame_type
          >> frame_.format
          >> rows >> cols >> type;
      // for debug
      //std::cerr << "tval: " << tval << " rows: " << rows << " cols: " << cols
      //          << " type:" << type << " format:" << frame_.format << std::endl;
      cv::Mat(rows, cols, type, const_cast<void *>(buf.raw_data(buf.read_cursor())))
        .copyTo(frame_.data);
      // Convert the int value to enum FrameType
      frame_.frame_type = static_cast<enum FrameType>(frame_type);
      // Convert the usec time point to the system_clock time point
      auto usec = microseconds{tval};
      frame_.timestamp = time_point<system_clock>(usec);
      if (!consume_kernel_buffer_){
        break;
      }
    }
  }catch(const std::exception &e) {
    if (!count) {
      std::cerr << "Failed to receive a frame: " << e.what() << std::endl;
      return false;
    }
  }
  if (!count) {
    std::cerr << "No frame available to receive" << std::endl;
    return false;
  }
  return true;
}

bool convert_frame_to_images(const basic_frame& frame_, cv::Mat& gray_, cv::Mat& bgr_){
  if (frame_.format == "I420") {
    // i.e. cv::cvtColor(frame.data,gray,cv::COLOR_YUV2GRAY_I420);
    gray_ = frame_.data.rowRange(0, frame_.data.rows / 3 * 2);
    cv::cvtColor(frame_.data, bgr_, cv::COLOR_YUV2BGR_I420);
    return true;
  }else if (frame_.format == "BGR") {
    cv::cvtColor(frame_.data, gray_, cv::COLOR_BGR2GRAY);
    bgr_ = frame_.data;
    return true;
  }else if (frame_.format == "RGBA") {
    cv::cvtColor(frame_.data, gray_, cv::COLOR_RGBA2GRAY);
    cv::cvtColor(frame_.data, bgr_, cv::COLOR_RGBA2BGR);
    return true;
  }else{
    std::cerr << "Unsupported format: " << frame_.format << std::endl;
    return false;
  }
}


// Compile-time ZeroMQ configurations
#define ZMQ_HWM_IMAGE_INTAKE            1
#define ZMQ_SCORER_PROTOCOL_VER         "1"
#define ZMQ_IMAGE_TOPIC_NAME            "Image"
#define ZMQ_NUM_IO_THREADS              1
#define ZMQ_POLL_WAIT                   100     // msec

}
#endif // ZMQ_RECEIVER_HPP