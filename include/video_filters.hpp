#ifndef TBB_FILTER_HPP
#define TBB_FILTER_HPP

#include <string>
#include <vector>
#include <utility>
#include <tbb/pipeline.h>
#include <opencv2/core.hpp>

namespace video {

using namespace std::literals::string_literals;
using std::string;
using std::vector;

class VideoSource: public tbb::filter {
public:
  const vector<string> INPUT_VIDEO_PATH;
  const bool USE_SKIP;
  const bool USE_RESIZE;
  const float RESIZE_SCALE;
  cv::Size DEST_SIZE;
  double fps = 0.0;
  size_t reader_idx = 0;
  size_t frame_idx = 0;
private:
  vector<cv::VideoCapture> readers;
public:
  VideoSource(const vector<string> INPUT_VIDEO_PATH, const bool USE_SKIP, const float RESIZE_SCALE)
  : filter(tbb::filter::mode::serial_in_order)
  , INPUT_VIDEO_PATH{INPUT_VIDEO_PATH}
  , USE_SKIP{USE_SKIP}
  , USE_RESIZE{!(0.999 < RESIZE_SCALE && RESIZE_SCALE < 1.001)}
  , RESIZE_SCALE{RESIZE_SCALE}
  {
    int width = 0;
    int height = 0;
    for(auto path: INPUT_VIDEO_PATH){
      auto gst_cmd = "filesrc location="s + path + " ! decodebin ! videoconvert ! appsink sync=false";
      std::cerr << gst_cmd << "\n";
      auto reader = cv::VideoCapture{gst_cmd};
      if(!reader.isOpened()){ throw std::runtime_error{"cannot open reader "s + gst_cmd}; }
      int _width = reader.get(CV_CAP_PROP_FRAME_WIDTH);
      int _height = reader.get(CV_CAP_PROP_FRAME_HEIGHT);
      double _fps = reader.get(CV_CAP_PROP_FPS);
      std::cerr << "frame w,h,fps: " << _width << "," << _height << "," << _fps << "\n";
      if(width != 0 && width != _width){ throw std::runtime_error{"width doesnt match("s + std::to_string(width) + "," + std::to_string(_width) + ")"}; }
      if(height != 0 && height != _height){ throw std::runtime_error{"height doesnt match("s + std::to_string(height) + "," + std::to_string(_height) + ")"}; }
      if(fps != 0 && fps != _fps){
        std::cerr << "warning: fps isnt match. using round\n";
        if(std::round(fps) != std::round(_fps)){ throw std::runtime_error{"fps doesnt match("s + std::to_string(fps) + "," + std::to_string(_fps) + ")"}; }
        _fps = std::pow(std::round(fps) - fps, 2) > std::pow(std::round(fps) - _fps, 2) ? _fps : fps;
        std::cerr << "warning: " << _fps << " fps adopted\n";
      }
      width = _width;
      height = _height;
      fps = _fps;
      readers.push_back(reader);
    }
    std::cerr << "USE_SKIP: " << USE_SKIP << "\n";
    if(USE_SKIP){ fps = fps/2.0; }
    std::cerr << "finaly w,h,fps: " << width << "," << height << "," << fps << "\n";
    std::cerr << "RESIZE_SCALE: " << RESIZE_SCALE << "\n";
    std::cerr << "USE_RESIZE: " << USE_RESIZE << "\n";
    DEST_SIZE = !USE_RESIZE ? cv::Size{width, height}
              :               cv::Size{
                                static_cast<int>(std::lround(RESIZE_SCALE*static_cast<float>(width))),
                                static_cast<int>(std::lround(RESIZE_SCALE*static_cast<float>(height))) };
    std::cerr << "DEST_SIZE: " << DEST_SIZE.width << "," << DEST_SIZE.height << "\n";
  }
  void* operator()(void*){
    while(true){
      auto reader = readers[reader_idx];
      auto mat = new cv::Mat{};
    if(reader.read(*mat)){
        frame_idx++;
        if(USE_SKIP && ((frame_idx % 2) == 0)){
          delete mat;
          continue;
        }
        if(USE_RESIZE){
          auto tmp = new cv::Mat{};
          cv::resize(*mat, *tmp, DEST_SIZE);
          delete mat;
          mat = tmp;
        }
        return static_cast<void*>(mat);
      }
      delete mat;
      // reader consumed
      std::cerr << INPUT_VIDEO_PATH[reader_idx] << " finished\n";
      if(readers.size() != reader_idx + 1){
        reader_idx++;
        frame_idx = 0;
        continue;
      }
      std::cerr << "all readers finished\n";
      return nullptr;
    }
  }
};

class VideoSink: public tbb::filter {
private:
  cv::VideoWriter writer;
public:
  VideoSink(const string& OUTPUT_VIDEO_PATH, const double fps, const cv::Size DEST_SIZE)
  : filter{tbb::filter::mode::serial_in_order} {
    std::cerr << "OUTPUT_VIDEO_PATH: " << OUTPUT_VIDEO_PATH << "\n";
    auto gst_cmd = "appsrc ! videoconvert ! x264enc ! video/x-h264,profile=main ! mp4mux ! filesink sync=false location="s + OUTPUT_VIDEO_PATH + " ";
    std::cout << gst_cmd << "\n";
    writer = cv::VideoWriter(gst_cmd, 0, fps, DEST_SIZE, true);
    if(!writer.isOpened()){ throw std::runtime_error{"cannot not open writer :"s + gst_cmd}; }
  }
  void* operator()(void* ptr){
    if(ptr == nullptr){ std::cerr << "writer got nullptr\n"; return nullptr; }
    auto src = static_cast<cv::Mat*>(ptr);
    writer.write(*src);
    delete src;
    return nullptr;
  }
};

}
#endif // TBB_FILTER_HPP