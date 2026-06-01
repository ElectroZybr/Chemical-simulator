#include "FfmpegStreamer.h"

#include <cassert>
#include <csignal>
#include <sstream>

#include "Lattice/Engine/metrics/Profiler.h"

FFmpegStreamer::~FFmpegStreamer() {
    if (running_) {
        stop();
    }
}

bool FFmpegStreamer::start(uint32_t width, uint32_t height, const char* inputPixFmt, const CaptureSettings& settings,
                           const std::filesystem::path& outputPath) {
    std::string cmd = buildCommand(width, height, inputPixFmt, settings, outputPath);

#ifdef _WIN32
    pipe_ = _popen(cmd.data(), "wb");
#else
    std::signal(SIGPIPE, SIG_IGN);
    pipe_ = popen(cmd.data(), "w");
#endif

    if (!pipe_) {
        return false;
    }

    running_ = true;
    writerThread_ = std::thread(&FFmpegStreamer::writerLoop, this);
    return true;
}

void FFmpegStreamer::stop() {
    {
        std::lock_guard lock(mutex_);
        running_ = false;
    }
    cv_.notify_one();

    if (writerThread_.joinable()) {
        writerThread_.join();
    }

    if (pipe_) {
#ifdef _WIN32
        _pclose(pipe_);
#else
        pclose(pipe_);
#endif
        pipe_ = nullptr;
    }
}

bool FFmpegStreamer::pushFrame(std::vector<std::byte> pixels) {
    std::lock_guard lock(mutex_);

    if (!running_ || pipe_ == nullptr || queue_.size() >= kMaxQueueDepth) {
        return false;
    }

    queue_.push_back(std::move(pixels));
    cv_.notify_one();
    return true;
}

void FFmpegStreamer::writerLoop() {
    while (true) {
        std::vector<std::byte> frame;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            if (queue_.empty()) {
                break;
            }

            frame = std::move(queue_.front());
            queue_.pop_front();
        }

        PROFILE_SCOPE("FFmpegStreamer::writeFrame");
        const size_t written = fwrite(frame.data(), 1, frame.size(), pipe_);
        if (written != frame.size() || fflush(pipe_) != 0) {
            std::lock_guard lock(mutex_);
            running_ = false;
            break;
        }
    }
}

std::string FFmpegStreamer::buildCommand(uint32_t width, uint32_t height, const char* inputPixFmt, const CaptureSettings& settings,
                                         const std::filesystem::path& outputPath) const {
    using namespace capture_utils;
    std::ostringstream cmd;
    cmd << "ffmpeg -y -loglevel error"
        << " -f rawvideo"
        << " -pix_fmt " << inputPixFmt << " -s " << width << "x" << height << " -r " << settings.fps << " -i pipe:0"
        << " -vcodec libx264"
        << " -crf " << settings.crf << " -preset " << toPresetArg(settings.preset) << " -pix_fmt " << toPixelFormatArg(settings.pixelFormat)
        << " \"" << outputPath.string() << "\"";
    return cmd.str();
}
