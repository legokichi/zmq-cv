#include <ffmpeg_io.h>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

using std::vector;
using std::string;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

/** https://gist.github.com/yohhoy/f0444d3fc47f2bb2d0e2 */
class FFmpegReader {
public:
  int dst_width;
  int dst_height;
  double dst_fps;
private:
  vector<uint8_t> framebuf;
  AVFrame* decframe = nullptr;
  AVFrame* frame = nullptr;
  AVStream* vstrm = nullptr;
  AVFormatContext* inctx = nullptr;
  SwsContext* swsctx = nullptr;
  int vstrm_idx;
  unsigned nb_frames = 0;
  bool end_of_stream = false;
  int got_pic = 0;
  AVPacket pkt;
public:
  FFmpegReader(const string filename){
    av_register_all();
    // open input file context
    if (auto ret = avformat_open_input(&inctx, filename.c_str(), nullptr, nullptr); ret < 0) {
      std::cerr << "fail to avforamt_open_input(\"" << filename << "\"): ret=" << ret;
      throw;
    }
    // retrive input stream information
    if (auto ret = avformat_find_stream_info(inctx, nullptr); ret < 0) {
      std::cerr << "fail to avformat_find_stream_info: ret=" << ret;
      throw;
    }
    // find primary video stream
    AVCodec* vcodec = nullptr;
    vstrm_idx = av_find_best_stream(inctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
    if (vstrm_idx < 0) {
      std::cerr << "fail to av_find_best_stream: ret=" << vstrm_idx;
      throw;
    }
    vstrm = inctx->streams[vstrm_idx];

    // open video decoder context
    if (auto ret = avcodec_open2(vstrm->codec, vcodec, nullptr); ret < 0) {
      std::cerr << "fail to avcodec_open2: ret=" << ret;
      throw;
    }
    // print input video stream informataion
    std::cout
      << "infile: " << filename << "\n"
      << "format: " << inctx->iformat->name << "\n"
      << "vcodec: " << vcodec->name << "\n"
      << "size:   " << vstrm->codec->width << 'x' << vstrm->codec->height << "\n"
      << "fps:    " << av_q2d(vstrm->codec->framerate) << " [fps]\n"
      << "length: " << av_rescale_q(vstrm->duration, vstrm->time_base, {1,1000}) / 1000. << " [sec]\n"
      << "pixfmt: " << av_get_pix_fmt_name(vstrm->codec->pix_fmt) << "\n"
      << "frame:  " << vstrm->nb_frames << "\n"
      << std::flush;
    // initialize sample scaler
    dst_width = vstrm->codec->width;
    dst_height = vstrm->codec->height;
    dst_fps = av_q2d(vstrm->codec->framerate);
    const AVPixelFormat dst_pix_fmt = AV_PIX_FMT_BGR24;
    swsctx = sws_getCachedContext(nullptr, vstrm->codec->width, vstrm->codec->height, vstrm->codec->pix_fmt, dst_width, dst_height, dst_pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsctx) {
      std::cerr << "fail to sws_getCachedContext";
      throw;
    }
    std::cout << "output: " << dst_width << 'x' << dst_height << ',' << av_get_pix_fmt_name(dst_pix_fmt) << std::endl;
    // allocate frame buffer for output
    frame = av_frame_alloc();
    framebuf = std::vector<uint8_t>(avpicture_get_size(dst_pix_fmt, dst_width, dst_height));
    avpicture_fill(reinterpret_cast<AVPicture*>(frame), framebuf.data(), dst_pix_fmt, dst_width, dst_height);
    // decoding loop
    decframe = av_frame_alloc();
  }
  ~FFmpegReader(){
    //sws_freeContext(swsctx);
    av_frame_free(&decframe);
    av_frame_free(&frame);
    avcodec_close(vstrm->codec);
    avformat_close_input(&inctx);
  }
  bool read(cv::Mat& mat){
    if (!end_of_stream) {
      // read packet from input file
      if (auto ret = av_read_frame(inctx, &pkt); ret < 0 && ret != AVERROR_EOF) {
        std::cerr << "fail to av_read_frame: ret=" << ret;
        throw;
      }else if (ret == 0 && pkt.stream_index != vstrm_idx){
        av_free_packet(&pkt);
        return read(mat); // go to next packet
      }else{
        end_of_stream = (ret == AVERROR_EOF);
      }
    }
    if (end_of_stream) {
      // null packet for bumping process
      av_init_packet(&pkt);
      pkt.data = nullptr;
      pkt.size = 0;
    }
    // decode video frame
    avcodec_decode_video2(vstrm->codec, decframe, &got_pic, &pkt);
    if (got_pic){
      // convert frame to OpenCV matrix
      sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);
      mat = cv::Mat{dst_height, dst_width, CV_8UC3, framebuf.data(), frame->linesize[0]}.clone();
      //std::cout << nb_frames << '\r' << std::flush;  // dump progress
      ++nb_frames;
      av_free_packet(&pkt);
      return true;
    }else{
      av_free_packet(&pkt);
      return false;
    }
  }
};

/** https://gist.github.com/yohhoy/52b31522dbb751e5296e */
class FFmpegWriter {
private:
  vector<uint8_t> imgbuf;
  vector<uint8_t> framebuf;
  cv::Mat image;
  AVFrame* frame = nullptr;
  AVFormatContext* outctx = nullptr;
  AVCodec* vcodec = nullptr;
  AVStream* vstrm = nullptr;
  SwsContext* swsctx = nullptr;
  int64_t frame_pts = 0;
  unsigned nb_frames = 0;
  int got_pkt = 0;
  bool end_of_stream = false;
public:
  FFmpegWriter(const string filename, const int fps, const cv::Size& size){
    auto dst_width = size.width;
    auto dst_height = size.height;
    auto dst_fps = AVRational{fps, 1};
    // initialize FFmpeg library
    av_register_all();
    // allocate cv::Mat with extra bytes (required by AVFrame::data)
    imgbuf = vector<uint8_t>(dst_height * dst_width * 3 + 16);
    image = cv::Mat{dst_height, dst_width, CV_8UC3, imgbuf.data(), dst_width * 3};
    // open output format context
    if (auto ret = avformat_alloc_output_context2(&outctx, nullptr, nullptr, filename.c_str()); ret < 0) {
      std::cerr << "fail to avformat_alloc_output_context2(" << filename << "): ret=" << ret;
      throw;
    }
    // open output IO context
    if(auto ret = avio_open2(&outctx->pb, filename.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr); ret < 0) {
      std::cerr << "fail to avio_open2: ret=" << ret;
      throw;
    }
    // create new video stream
    vcodec = avcodec_find_encoder(outctx->oformat->video_codec);
    vstrm = avformat_new_stream(outctx, vcodec);
    if (!vstrm) {
      std::cerr << "fail to avformat_new_stream";
      throw;
    }
    avcodec_get_context_defaults3(vstrm->codec, vcodec);
    vstrm->codec->width = dst_width;
    vstrm->codec->height = dst_height;
    vstrm->codec->pix_fmt = vcodec->pix_fmts[0];
    vstrm->codec->time_base = vstrm->time_base = av_inv_q(dst_fps);
    vstrm->r_frame_rate = vstrm->avg_frame_rate = dst_fps;
    if (outctx->oformat->flags & AVFMT_GLOBALHEADER){
      vstrm->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    // open video encoder
    if (auto ret = avcodec_open2(vstrm->codec, vcodec, nullptr); ret < 0) {
      std::cerr << "fail to avcodec_open2: ret=" << ret;
      throw;
    }
    std::cout
      << "outfile: " << filename << "\n"
      << "format:  " << outctx->oformat->name << "\n"
      << "vcodec:  " << vcodec->name << "\n"
      << "size:    " << dst_width << 'x' << dst_height << "\n"
      << "fps:     " << av_q2d(dst_fps) << "\n"
      << "pixfmt:  " << av_get_pix_fmt_name(vstrm->codec->pix_fmt) << "\n"
      << std::flush;
    // initialize sample scaler
    swsctx = sws_getCachedContext(nullptr, dst_width, dst_height, AV_PIX_FMT_BGR24, dst_width, dst_height, vstrm->codec->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsctx) {
      std::cerr << "fail to sws_getCachedContext";
      throw;
    }
    // allocate frame buffer for encoding
    frame = av_frame_alloc();
    framebuf = vector<uint8_t>(avpicture_get_size(vstrm->codec->pix_fmt, dst_width, dst_height));
    avpicture_fill(reinterpret_cast<AVPicture*>(frame), framebuf.data(), vstrm->codec->pix_fmt, dst_width, dst_height);
    frame->width = dst_width;
    frame->height = dst_height;
    frame->format = static_cast<int>(vstrm->codec->pix_fmt);
    // encoding loop
    avformat_write_header(outctx, nullptr);
  }
  ~FFmpegWriter(){
    //sws_freeContext(swsctx);
    av_frame_free(&frame);
    avcodec_close(vstrm->codec);
    avio_close(outctx->pb);
    avformat_free_context(outctx);
  }
  void write(const cv::Mat& mat){
    if(end_of_stream){ std::cerr << "eof" << std::endl; return; }
    // retrieve source image
    mat.copyTo(image);
    // convert cv::Mat(OpenCV) to AVFrame(FFmpeg)
    const int stride[] = { static_cast<int>(image.step[0]) };
    sws_scale(swsctx, &image.data, stride, 0, image.rows, frame->data, frame->linesize);
    frame->pts = frame_pts++;
    if(!encode()){
      end_of_stream = true;
    }
  }
  void end(){
    std::cout << "end_of_stream" << std::endl;
    end_of_stream = true;
    std::cout << "encode" << std::endl;
    while(encode()){ "noop"; };
    std::cout << "av_write_trailer" << std::endl;
    av_write_trailer(outctx);
    std::cout << nb_frames << " frames encoded" << std::endl;
  }
private:
  bool encode(){
    // encode video frame
    AVPacket pkt;
    pkt.data = nullptr;
    pkt.size = 0;
    av_init_packet(&pkt);
    if (auto ret = avcodec_encode_video2(vstrm->codec, &pkt, end_of_stream ? nullptr : frame, &got_pkt); ret < 0) {
      std::cerr << "fail to avcodec_encode_video2: ret=" << ret << "\n";
      av_free_packet(&pkt);
      return false;
    }
    if (got_pkt) {
      // rescale packet timestamp
      pkt.duration = 1;
      av_packet_rescale_ts(&pkt, vstrm->codec->time_base, vstrm->time_base);
      // write packet
      av_write_frame(outctx, &pkt);
      //std::cout << nb_frames << '\r' << std::flush;  // dump progress
      ++nb_frames;
    }
    av_free_packet(&pkt);
    return got_pkt ? true : false;
  }
};



ffmpeg_reader_t* ffmpeg_reader_create(char* filename){
  return static_cast<ffmpeg_reader_t*>(new FFmpegReader{string{filename}});
}
void ffmpeg_reader_destroy(ffmpeg_reader_t* ptr){
  delete static_cast<FFmpegReader*>(ptr);
}
bool ffmpeg_reader_read(ffmpeg_reader_t* ptr, cv::Mat* mat){
  return static_cast<FFmpegReader*>(ptr)->read(*mat);
}
cv::Size ffmpeg_reader_get_size(ffmpeg_reader_t* ptr){
  auto& reader = *static_cast<FFmpegReader*>(ptr);
  return cv::Size{reader.dst_width, reader.dst_height};
}
double ffmpeg_reader_get_fps(ffmpeg_reader_t* ptr){
  return static_cast<FFmpegReader*>(ptr)->dst_fps;
}

typedef void ffmpeg_writer_t;
ffmpeg_writer_t* ffmpeg_writer_create(char* filename, int fps, cv::Size size){
  return static_cast<ffmpeg_writer_t*>(new FFmpegWriter{string{filename}, fps, size});
}
void ffmpeg_writer_destroy(ffmpeg_writer_t* ptr){
  delete static_cast<FFmpegWriter*>(ptr);
}
void ffmpeg_writer_write(ffmpeg_writer_t* ptr, cv::Mat* mat){
  static_cast<FFmpegWriter*>(ptr)->write(*mat);
}
void ffmpeg_writer_end(ffmpeg_writer_t* ptr){
  static_cast<FFmpegWriter*>(ptr)->end();
}