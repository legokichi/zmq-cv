#include <Config.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#ifdef USE_FFMPEG
#include <ffmpeg_io.h>
#endif

using std::vector;
using std::string;
using std::shared_ptr;

int main(int argc, char* argv[]){
  string INPUT_VIDEO_PATH;
  string OUTPUT_VIDEO_PATH;
  namespace po = boost::program_options;
  po::options_description opt("option");
  opt.add_options()
    ("help,h", "show this menu")
    ("input,i", po::value<string>(&INPUT_VIDEO_PATH)->required(), "input video file path")
    ("output,o", po::value<string>(&OUTPUT_VIDEO_PATH)->required(), "output mp4 video file path if you need")
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
  if( vm.count("help")  ){
    std::cout << "Using OpenCV version " << CV_VERSION << "\n" << std::endl;
    std::cout << cv::getBuildInformation();
    std::cout << opt << std::endl; // put help
    return EXIT_FAILURE;
  }
#ifdef USE_FFMPEG
  auto reader_ptr = shared_ptr<ffmpeg_reader_t>{ffmpeg_reader_create(const_cast<char*>(INPUT_VIDEO_PATH.c_str())), ffmpeg_reader_destroy};
  auto fps = ffmpeg_reader_get_fps(reader_ptr.get());
  auto DEST_SIZE = ffmpeg_reader_get_size(reader_ptr.get());
  auto writer_ptr = shared_ptr<ffmpeg_writer_t>{ffmpeg_writer_create(const_cast<char*>(OUTPUT_VIDEO_PATH.c_str()), static_cast<int>(fps), DEST_SIZE), ffmpeg_writer_destroy};
#else
  auto reader = cv::VideoCapture{INPUT_VIDEO_PATH};
  if(!reader.isOpened()){
    printf("C++: reader cannot opened\n");
    throw;
  }
  int width = reader.get(CV_CAP_PROP_FRAME_WIDTH);
  int height = reader.get(CV_CAP_PROP_FRAME_HEIGHT);
  double fps = reader.get(CV_CAP_PROP_FPS);
  auto DEST_SIZE = cv::Size{width, height};
  auto writer = cv::VideoWriter("appsrc ! videoconvert ! x264enc ! video/x-h264,profile=main ! mp4mux ! filesink location=" + OUTPUT_VIDEO_PATH + "  ", 0, fps, DEST_SIZE, true);
  if(!writer.isOpened()){
    printf("C++: writer not opened\n");
    throw;
  }
#endif
  cv::Mat mat;
  while(true){
    bool ret = false;
    {
      //auto start = std::chrono::system_clock::now();
#ifdef USE_FFMPEG
      ret = ffmpeg_reader_read(reader_ptr.get(), &mat);
#else
      ret = reader.read(mat);
#endif
      //auto stop = std::chrono::system_clock::now();
      //auto msec = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
      //std::cout << "r" << msec << "\n";
    }
    if(!ret){ break; }
    {
      //auto start = std::chrono::system_clock::now();
#ifdef USE_FFMPEG
      ffmpeg_writer_write(writer_ptr.get(), &mat);
#else
      writer.write(mat);
#endif
      //auto stop = std::chrono::system_clock::now();
      //auto msec = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
      //std::cout << "w" << msec << "\n";
    }
  }
#ifdef USE_FFMPEG
  std::cout << "end" << std::endl;
  ffmpeg_writer_end(writer_ptr.get());
#endif
  std::cout << "finished" << std::endl;
  return EXIT_SUCCESS;
}