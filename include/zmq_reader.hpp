#ifndef ZMQ_READER_HPP
#define ZMQ_READER_HPP

#include <zmqpp/zmqpp.hpp>
#include <opencv2/core.hpp>
#include <chrono>
#include <utility>
#include <optional>
#include <zmq_reciever.hpp>

namespace zmq {

using namespace std::literals::string_literals;
using std::string;
namespace chrono = std::chrono;
using std::pair;
using std::optional;
using std::unique_ptr;

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
  const uint32_t timeout = 10; // Quit if no frame arrives for timeout seconds
  uint32_t duration = 0; // Quit after duration seconds of running
public:
  uint32_t frame_index = 0;
public:
  ZMQReader(const zmq_source_socktype_t zmq_type, const string& endpoint, const uint32_t timeout)
  : timeout{timeout}{
    zmqpp::socket_type image_stype;
    switch(zmq_type){
      case SOCKTYPE_SUB: image_stype = zmqpp::socket_type::subscribe; break;
      case SOCKTYPE_PULL: image_stype = zmqpp::socket_type::pull; break;
      default: throw std::runtime_error{"no supported socktype"};
    }
    ctx = std::make_unique<zmqpp::context>();
    ctx->set(zmqpp::context_option::io_threads, ZMQ_NUM_IO_THREADS);
    sock_image = std::make_unique<zmqpp::socket>(*ctx, image_stype);
    if (sock_image->type() == zmqpp::socket_type::subscribe){ sock_image->subscribe(ZMQ_IMAGE_TOPIC_NAME); }
    sock_image->set(zmqpp::socket_option::receive_high_water_mark, ZMQ_HWM_IMAGE_INTAKE);
    sock_image->connect(endpoint);
    poller.add(*sock_image, zmqpp::poller::poll_in);
    start_time = chrono::system_clock::now();
    last_activity = start_time;
  }
  ~ZMQReader(){}
  optional<pair<frame_timestamp, uint32_t>> read(cv::Mat& mat){
    optional<pair<frame_timestamp, uint32_t>> ret = std::nullopt;
    while (!duration || chrono::duration_cast<chrono::seconds>(elapsed).count() < duration) {
      auto now = chrono::system_clock::now();
      elapsed = chrono::duration_cast<chrono::milliseconds>(now - start_time);
      // Wait for the new image to arrive; false means a poll timeout
      //std::cerr << "poll" << std::endl;
      if (!poller.poll(ZMQ_POLL_WAIT)) {
        if (!timeout){ std::cerr << "not timeout" << std::endl; continue; }
        auto waiting = now - last_activity;
        if (chrono::duration_cast<chrono::seconds>(waiting).count() > timeout && frame_index > 0) {
          std::cerr << "Quitting due to inactivity timeout" << std::endl;
          break;
        }
        //std::cerr << "no poll " << frame_index << std::endl;
        continue;
      }
      // queue found
      last_activity = now;
      if (!(poller.events(*sock_image) & zmqpp::poller::poll_in)){ std::cerr << "not found" << std::endl; continue; }
      basic_frame packet;
      if (!receive_image_frame(*sock_image, packet)){ std::cerr << "receive_image_frame failed" << std::endl; continue; }
      frame_index += 1;
      cv::Mat gray;
      if(!convert_frame_to_images(packet, gray, mat)){ std::cerr << "convert_frame_to_images failed" << packet.format << std::endl; continue; }
      uint32_t _frame_index = frame_index-1;
      ret = std::make_optional(std::make_pair(packet.timestamp, _frame_index));
      break;
    }
    return ret;
  }
};

}
#endif // ZMQ_READER_HPP