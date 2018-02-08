#ifndef ZMQ_VIDEO_CAPTURE_HPP
#define ZMQ_VIDEO_CAPTURE_HPP
#include <opencv2/core.hpp>
#include <sstream>
#include <optional>
#include <chrono>
#include <algorithm>
#include <tuple>
#include <utility>
#include <opencv2/opencv.hpp>
#include <zmqpp/zmqpp.hpp>
#include <date.h>

namespace zmq {

using std::string;
namespace chrono = std::chrono;
using std::tuple;
using std::pair;
using std::vector;
using std::optional;
using std::unique_ptr;
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


typedef enum zmq_source_socktype_t { SOCKTYPE_PULL, SOCKTYPE_SUB } zmq_source_socktype_t;


class ZMQReader{
private:
  unique_ptr<zmqpp::context> ctx;
  unique_ptr<zmqpp::socket> sock_image;
  zmqpp::poller poller;
  // Setup timer stuff
  chrono::time_point<chrono::system_clock, chrono::nanoseconds> start_time;
  chrono::time_point<chrono::system_clock, chrono::nanoseconds> last_activity;
  chrono::milliseconds elapsed{0};
  uint32_t timeout = 10; // Quit if no frame arrives for timeout seconds
  uint32_t duration = 0; // Quit after duration seconds of running
public:
  uint32_t frame_index = 0;
public:
  ZMQReader(zmq_source_socktype_t zmq_type, const string& endpoint, uint32_t timeout_sec){
    zmqpp::socket_type image_stype;
    switch(zmq_type){
      case SOCKTYPE_SUB: image_stype = zmqpp::socket_type::subscribe; break;
      case SOCKTYPE_PULL: image_stype = zmqpp::socket_type::pull; break;
      default: throw std::runtime_error("no supported socktype");
    }
    auto o = this;
    o->ctx = std::make_unique<zmqpp::context>();
    o->ctx->set(zmqpp::context_option::io_threads, ZMQ_NUM_IO_THREADS);
    o->sock_image = std::make_unique<zmqpp::socket>(*o->ctx, image_stype);
    if (o->sock_image->type() == zmqpp::socket_type::subscribe){ o->sock_image->subscribe(ZMQ_IMAGE_TOPIC_NAME); }
    o->sock_image->set(zmqpp::socket_option::receive_high_water_mark, ZMQ_HWM_IMAGE_INTAKE);
    o->sock_image->connect(endpoint);
    o->poller.add(*o->sock_image, zmqpp::poller::poll_in);
    o->start_time = chrono::system_clock::now();
    o->last_activity = o->start_time;
    o->timeout = timeout_sec;
  }
  ~ZMQReader(){}
  optional<pair<int64_t, uint32_t>> read(cv::Mat& mat){
    auto o = this;
    optional<pair<int64_t, uint32_t>> ret = std::nullopt;
    while (!o->duration || chrono::duration_cast<chrono::seconds>(o->elapsed).count() < o->duration) {
      auto now = chrono::system_clock::now();
      o->elapsed = chrono::duration_cast<chrono::milliseconds>(now - o->start_time);
      // Wait for the new image to arrive; false means a poll timeout
      //std::cerr << "poll" << std::endl;
      if (!o->poller.poll(ZMQ_POLL_WAIT)) {
        if (!o->timeout){ std::cerr << "not timeout" << std::endl; continue; }
        auto waiting = now - o->last_activity;
        if (chrono::duration_cast<chrono::seconds>(waiting).count() > o->timeout && o->frame_index > 0) {
          std::cerr << "Quitting due to inactivity timeout" << std::endl;
          break;
        }
        //std::cerr << "no poll " << o->frame_index << std::endl;
        continue;
      }
      // queue found
      o->last_activity = now;
      if (!(o->poller.events(*o->sock_image) & zmqpp::poller::poll_in)){ std::cerr << "not found" << std::endl; continue; }
      basic_frame packet;
      if (!receive_image_frame(*o->sock_image, packet)){ std::cerr << "receive_image_frame failed" << std::endl; continue; }
      o->frame_index += 1;
      cv::Mat gray;
      if(!convert_frame_to_images(packet, gray, mat)){ std::cerr << "convert_frame_to_images failed" << packet.format << std::endl; continue; }
      std::cout << "timestamp: " << packet.timestamp << std::endl;
      int64_t timestamp = chrono::duration_cast<chrono::milliseconds>(packet.timestamp.time_since_epoch()).count();
      uint32_t frame_index = o->frame_index-1;
      ret = std::make_optional(std::make_pair(timestamp, frame_index));
      break;
    }
    return ret;
  }
};

class VideoCapture {
public:
    double fps;
    int width;
    int height;
    int counter = 0;
    bool finish = false;
    int64_t timestamp;
    uint32_t frame_idx;
    string time_string;
private:
    ZMQReader reader;
    cv::Mat mat0;
    int64_t timestamp0;
    uint32_t frame_idx0;
    cv::Mat mat1;
    int64_t timestamp1;
    uint32_t frame_idx1;
public:
    VideoCapture(const string& zmq_endpoint, const int zmq_socktype, const int inactivity_timeout)
        : reader{zmq_socktype == 0 ? SOCKTYPE_PULL : SOCKTYPE_SUB, zmq_endpoint, static_cast<uint32_t>(inactivity_timeout)} {
        auto opt0 = reader.read(mat0);
        auto opt1 = reader.read(mat1);
        if(opt0 != std::nullopt && opt1 != std::nullopt){
            auto [_timestamp0, _frame_idx0] = *opt0;
            auto [_timestamp1, _frame_idx1] = *opt1;
            std::cout << _timestamp0 << "," << _frame_idx0 << "," << mat0.size().width << "x" << mat0.size().height << std::endl;
            std::cout << _timestamp1 << "," << _frame_idx1 << "," << mat1.size().width << "x" << mat1.size().height << std::endl;
            auto duration_ms = (timestamp1 - timestamp0);
            fps = 1000 / duration_ms;
            width = mat1.size().width;
            height = mat1.size().height;
            frame_idx0 = _frame_idx0;
            timestamp0 = _timestamp0;
            frame_idx1 = _frame_idx1;
            timestamp1 = _timestamp1;
        }else{
            throw std::runtime_error{"cannot get first 2 frames"};
        }
    }
    ~VideoCapture(){}
    inline bool isOpened() const { return !finish; }
    void release(){}
    double get(const int capProperty){
        if (capProperty == CV_CAP_PROP_FRAME_WIDTH){
            return width;
        }else if(capProperty == CV_CAP_PROP_FRAME_HEIGHT){
            return height;
        }else if(capProperty == CV_CAP_PROP_FPS){
            return fps;
        }else if(capProperty == CV_CAP_PROP_POS_FRAMES){
            return reader.frame_index;
        }else if(capProperty == CV_CAP_PROP_FRAME_COUNT){
            return -1;
        }else{
            std::cerr << "unknown prop " << capProperty << std::endl;
            return 0;
        }
    }
    void set(const int capProperty, const double value){
        std::cerr << "set(" << capProperty << "," << value << ")" << std::endl;
    }
    VideoCapture& operator>>(cv::Mat &mat){
        std::stringstream strm;
        // strm << dt;
        // strm.str();
        if(counter == 0){ timestamp = timestamp0; frame_idx = frame_idx0; /*time_string = convertUnixTimeToStringFormat(timestamp0);*/ counter++; mat = mat0; return *this; }
        if(counter == 1){ timestamp = timestamp1; frame_idx = frame_idx1; /*time_string = convertUnixTimeToStringFormat(timestamp1);*/ counter++; mat = mat1; return *this; }
        if(auto opt = reader.read(mat); opt != std::nullopt){
            auto [_timestamp, _frame_idx] = *opt;
            timestamp = _timestamp; // unix_millis
            /*time_string = convertUnixTimeToStringFormat(_timestamp);*/
            frame_idx = _frame_idx;
            return *this;
        }
        std::cout << "finished" << std::endl;
        finish = true;
        return *this;
    }
};


}
#endif // ZMQ_VIDEO_CAPTURE_HPP