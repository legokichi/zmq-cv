#include <iostream>
#include <fstream>
#include <utility>
#include <vector>
#include <tuple>
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include <tbb/pipeline.h>
#include <tbb_filters.hpp>
#include <zmq_filters.hpp>

using namespace std::literals::string_literals;
using std::string;
using std::tuple;
using std::vector;
using std::thread;
using zmq::zmq_source_socktype_t;

namespace zmq {
  class GPUFilter: public tbb::thread_bound_filter {
  private:
    std::ofstream fout;
    // const shared_ptr<foo> foo_ptr;
  public:
    GPUFilter(const cv::Size DEST_SIZE, const int GPU_ID, const string& OUTPUT_LOG_PATH)
    : thread_bound_filter(tbb::filter::mode::serial_in_order)
    , fout{OUTPUT_LOG_PATH}
    // , foo_ptr{foo_create(DEST_SIZE.width, DEST_SIZE.height, GPU_ID), foo_destroy}
    {
      // if(foo_ptr == nullptr){ throw std::runtime_error{"cannot initialize foo"}; }
    }
    void* operator()(void* ptr){
      if(ptr == nullptr){ std::cerr << "sense got nullptr" << std::endl; return nullptr; }
      auto _ptr = static_cast<tuple<cv::Mat*, frame_timestamp, uint32_t>*>(ptr);
      auto [src, timestamp, frame_index] = *_ptr;
      delete _ptr;
      using namespace date;
      fout << timestamp << "\n";
      //auto ret = foo_read(foo_ptr.get(), src);
      return static_cast<void*>(src);
    }
  };
}

int main(int argc, char* argv[]){
  int GPU_ID;
  string ZMQ_FRAME_GRABBER_ENDPOINT;
  string ZMQ_FRAME_GRABBER_SOCKTYPE;
  int INACTIVITY_TIMEOUT;
  string OUTPUT_VIDEO_PATH;
  string OUTPUT_LOG_PATH;

  namespace po = boost::program_options;
  po::options_description opt("option");
  opt.add_options()
    ("help,h", "show this menu")
    ("endpoint", boost::program_options::value<string>(&ZMQ_FRAME_GRABBER_ENDPOINT)->default_value("ipc://@/scorer/frame_grabber-video0"), "ZMQ_FRAME_GRABBER_ENDPOINT")
    ("socktype", boost::program_options::value<string>(&ZMQ_FRAME_GRABBER_SOCKTYPE)->default_value("PULL"), "ZMQ_FRAME_GRABBER_SOCKTYPE = PULL|SUB")
    ("timeout", boost::program_options::value<int>(&INACTIVITY_TIMEOUT)->default_value(3), "INACTIVITY_TIMEOUT")
    ("output,o", po::value<string>(&OUTPUT_VIDEO_PATH)->required(), "output mp4 video file path")
    ("output_log", boost::program_options::value<string>(&OUTPUT_LOG_PATH)->required(), "OUTPUT_LOG_PATH")
    ("gpu", po::value<int>(&GPU_ID)->default_value(0), "using gpu id")
  ;
  auto vm = po::variables_map{};
  try{
    po::store(po::parse_command_line(argc, argv, opt), vm);
    po::notify(vm);
  }catch(std::exception& e){
    std::cerr << "Error: " << e.what() << "\n";
    std::cerr << opt << std::endl; // put help
    return EXIT_FAILURE;
  }
  std::cerr << "ZMQ_FRAME_GRABBER_ENDPOINT: " << ZMQ_FRAME_GRABBER_ENDPOINT << "\n";
  std::cerr << "ZMQ_FRAME_GRABBER_SOCKTYPE: " << ZMQ_FRAME_GRABBER_SOCKTYPE << "\n";
  std::cerr << "INACTIVITY_TIMEOUT: " << INACTIVITY_TIMEOUT << "\n";
  std::cerr << "OUTPUT_VIDEO_PATH: " << OUTPUT_VIDEO_PATH << "\n";
  std::cerr << "OUTPUT_LOG_PATH: " << OUTPUT_LOG_PATH << "\n";
  std::cerr << "GPU_ID: " << GPU_ID << "\n";

  tbb::pipeline pipe;
  auto source = zmq::VideoSource{ZMQ_FRAME_GRABBER_ENDPOINT, ZMQ_FRAME_GRABBER_SOCKTYPE, INACTIVITY_TIMEOUT};
  auto filter = zmq::GPUFilter{source.reader.size, GPU_ID, OUTPUT_LOG_PATH};
  auto sink = gst::VideoSink{OUTPUT_VIDEO_PATH, source.reader.fps, source.reader.size};
  pipe.add_filter(source);
  pipe.add_filter(filter);
  pipe.add_filter(sink);

  // pipe process run in another thread
  auto t = thread{([&](){ pipe.run(16); })};

  // gpu worker run in main thread
  while (filter.process_item() != tbb::thread_bound_filter::end_of_stream){ continue; }

  // waiting all worker stoppped
  t.join();
  std::cerr << "EXIT_SUCCESS" << std::endl;
  return EXIT_SUCCESS;
}