#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include <tbb/pipeline.h>
#include <tbb_filters.hpp>

using std::string;
using std::vector;
using std::thread;

int main(int argc, char* argv[]){
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
    ("skip", po::bool_switch(&USE_SKIP)->default_value(false), "1/2 fps")
    ("resize", po::value<float>(&RESIZE_SCALE)->default_value(1.0), "image resize scale")
  ;
  auto vm = po::variables_map{};
  try{
    po::store(po::parse_command_line(argc, argv, opt), vm);
    po::notify(vm);
  }catch(std::exception& e){
    std::cerr << "Error: " << e.what() << "\n";
    std::cout << opt << std::endl; // put help
    return EXIT_FAILURE;
  }

  tbb::pipeline pipe;
  auto source = Source{INPUT_VIDEO_PATH, USE_SKIP, RESIZE_SCALE};
  auto sink = Sink{OUTPUT_VIDEO_PATH, source.fps, source.DEST_SIZE};
  pipe.add_filter(source);
  pipe.add_filter(sink);
  auto t = thread{([&](){ pipe.run(16); })};
  t.join();
  std::cout << "EXIT_SUCCESS" << std::endl;
  return EXIT_SUCCESS;
}