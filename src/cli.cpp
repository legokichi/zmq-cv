#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include <tbb/pipeline.h>
#include <video_filters.hpp>

using std::string;
using std::tuple;
using std::vector;
using std::thread;

namespace gpu {
  class Processor: public tbb::thread_bound_filter {
  private:
  public:
    Processor(const cv::Size DEST_SIZE, const int GPU_ID)
    : thread_bound_filter{tbb::filter::mode::serial_in_order}
    {
    }
    void* operator()(void* ptr){
      if(ptr == nullptr){ std::cerr << "processor got nullptr\n"; return nullptr; }
      auto src = static_cast<cv::Mat*>(ptr);
      return static_cast<void*>(src);
    }
  };

  class Logger: public tbb::filter {
  private:
    std::ofstream fout;
  public:
    Logger(const string& OUTPUT_LOG_PATH)
    : filter{tbb::filter::mode::serial_in_order}
    , fout{OUTPUT_LOG_PATH}
    {
    }
    void* operator()(void* ptr){
      if(ptr == nullptr){ std::cerr << "logger got nullptr\n"; return nullptr; }
      auto src = static_cast<cv::Mat>(*ptr);
      //fout << timestamp << "\n";
      return static_cast<void*>(ptr);
    }
  };
}

int main(int argc, char* argv[]){
  int GPU_ID;
  vector<string> INPUT_VIDEO_PATH;
  string OUTPUT_VIDEO_PATH;
  string OUTPUT_LOG_PATH;
  bool USE_SKIP;
  float RESIZE_SCALE;
  namespace po = boost::program_options;
  po::options_description opt("option");
  opt.add_options()
    ("help,h", "show this menu")
    ("input,i", po::value<vector<string>>(&INPUT_VIDEO_PATH)->multitoken()->required(), "input video file path")
    ("output,o", po::value<string>(&OUTPUT_VIDEO_PATH)->required(), "output mp4 video file path if you need")
    ("gpu", po::value<int>(&GPU_ID)->default_value(0), "using gpu id")
    ("log", boost::program_options::value<string>(&OUTPUT_LOG_PATH)->default_value("log.tsv"), "OUTPUT_LOG_PATH")
    ("skip", po::bool_switch(&USE_SKIP)->default_value(false), "1/2 fps")
    ("resize", po::value<float>(&RESIZE_SCALE)->default_value(1.0), "image resize scale")
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

  tbb::pipeline pipe;
  auto source = video::VideoSource{INPUT_VIDEO_PATH, USE_SKIP, RESIZE_SCALE};
  auto processor = gpu::Processor{source.DEST_SIZE, GPU_ID};
  auto logger = gpu::Logger{OUTPUT_LOG_PATH};
  auto sink = video::VideoSink{OUTPUT_VIDEO_PATH, source.fps, source.DEST_SIZE};
  pipe.add_filter(source);
  pipe.add_filter(processor);
  pipe.add_filter(logger);
  pipe.add_filter(sink);

  // pipe process run in another thread
  auto t = thread{([&](){ pipe.run(16); })};

  // gpu worker run in main thread
  while (processor.process_item() != tbb::thread_bound_filter::end_of_stream){ continue; }

  // waiting all worker stoppped
  t.join();
  std::cerr << "EXIT_SUCCESS" << std::endl;
  return EXIT_SUCCESS;
}