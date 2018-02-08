#ifndef TBB_FILTER_HPP
#define TBB_FILTER_HPP

#include <string>
#include <vector>
#include <utility>
#include <tbb/pipeline.h>
#include <opencv2/core.hpp>

using std::string;
using std::vector;

class Source: public tbb::filter {
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
  Source(vector<string> INPUT_VIDEO_PATH, bool USE_SKIP, float RESIZE_SCALE)
  : filter(tbb::filter::mode::serial_in_order)
  , INPUT_VIDEO_PATH{INPUT_VIDEO_PATH}
  , USE_SKIP{USE_SKIP}
  , USE_RESIZE{!(0.999 < RESIZE_SCALE && RESIZE_SCALE < 1.001)}
  , RESIZE_SCALE{RESIZE_SCALE} {
    int width = 0;
    int height = 0;
    for(auto path: INPUT_VIDEO_PATH){
      auto reader = cv::VideoCapture{path};
      if(!reader.isOpened()){
        printf("C++: reader cannot opened\n");
        throw;
      }
      int _width = reader.get(CV_CAP_PROP_FRAME_WIDTH);
      int _height = reader.get(CV_CAP_PROP_FRAME_HEIGHT);
      double _fps = reader.get(CV_CAP_PROP_FPS);
      printf("%dx%d,%f\n", _width, _height, _fps);
      if(width != 0 && width != _width){ printf("width size isnt match!\n"); throw; }
      if(height != 0 && height != _height){ printf("height size isnt match!\n"); throw; }
      if(fps != 0 && fps != _fps){
        printf("warning: fps isnt match. using round\n");
        if(std::round(fps) != std::round(_fps)){ printf("error: fps isnt match(%f,%f)\n", fps, _fps); throw; }
        _fps = std::pow(std::round(fps) - fps, 2) > std::pow(std::round(fps) - _fps, 2) ? _fps : fps;
        printf("warning: %f fps adopted\n", _fps);
      }
      width = _width;
      height = _height;
      fps = _fps;
      readers.push_back(reader);
    }
    printf("USE_SKIP: %d\n", USE_SKIP);
    if(USE_SKIP){ fps = fps/2.0; }
    printf("finaly: %dx%d,%f\n", width, height, fps);
    printf("RESIZE_SCALE: %f\n", RESIZE_SCALE);
    printf("USE_RESIZE: %d\n", USE_RESIZE);
    DEST_SIZE = !USE_RESIZE ? cv::Size{width, height}
              :               cv::Size{static_cast<int>(std::lround(RESIZE_SCALE*static_cast<float>(width))), static_cast<int>(std::lround(RESIZE_SCALE*static_cast<float>(height)))};
    printf("DEST_SIZE: %d x %d\n", DEST_SIZE.width, DEST_SIZE.height);
  };
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
      std::cout << INPUT_VIDEO_PATH[reader_idx] << " finished" << std::endl;
      if(readers.size() != reader_idx + 1){
        reader_idx++;
        frame_idx = 0;
        continue;
      }
      std::cout << "all readers finished" << std::endl;
      return nullptr;
    }
  };
};

class Sink: public tbb::filter {
private:
  cv::VideoWriter writer;
public:
  Sink(const string& OUTPUT_VIDEO_PATH, const double fps, const cv::Size DEST_SIZE)
  : filter{tbb::filter::mode::serial_in_order} {
    std::cout << "OUTPUT_VIDEO_PATH: " << OUTPUT_VIDEO_PATH << std::endl;
    auto gstcom = "appsrc ! videoconvert ! x264enc ! video/x-h264,profile=main ! mp4mux ! filesink sync=false location=" + OUTPUT_VIDEO_PATH + " ";
    std::cout << "gst: " << gstcom << std::endl;
    writer = cv::VideoWriter(gstcom, 0, fps, DEST_SIZE, true);
    if(!writer.isOpened()){ printf("C++: writer not opened\n"); throw; }
  }
  void* operator()(void* ptr){
    if(ptr == nullptr){ std::cout << "writer got nullptr" << std::endl; return nullptr; }
    auto src = static_cast<cv::Mat*>(ptr);
    writer.write(*src);
    delete src;
    return nullptr;
  }
};

#endif // TBB_FILTER_HPP