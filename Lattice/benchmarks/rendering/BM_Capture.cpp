// #include <filesystem>
// #include <memory>

// #include <benchmark/benchmark.h>

// #include "App/capture/FrameRecorder.h"

// namespace {
//     std::filesystem::path makeCaptureBenchDir() {
//         const std::filesystem::path dir = std::filesystem::current_path() / "Benchmarks" / "results" / "capture_bench_tmp";
//         std::filesystem::create_directories(dir);
//         return dir;
//     }
// }

// class CaptureFixture : public benchmark::Fixture {
// public:
//     void SetUp(benchmark::State& state) override {
//         const uint32_t w = 800, h = 600;
//         capturedFrame_.width = w;
//         capturedFrame_.height = h;
//         capturedFrame_.rgba.resize(size_t(w) * h * 4, 128);

//         captureDir_ = makeCaptureBenchDir();
//         frameRecorder_ = std::make_unique<FrameRecorder>();
//         frameRecorder_->start(captureDir_ / "bench.mp4", CaptureSettings{});
//         if (!frameRecorder_->isRecording()) {
//             state.SkipWithError("Failed to start frame recorder");
//         }
//     }

//     void TearDown(benchmark::State&) override {
//         if (frameRecorder_) {
//             frameRecorder_->stop();
//             frameRecorder_.reset();
//         }
//         std::error_code ec;
//         std::filesystem::remove_all(captureDir_, ec);
//     }

// protected:
//     CapturedFrame capturedFrame_{};
//     std::unique_ptr<FrameRecorder> frameRecorder_{};
//     std::filesystem::path captureDir_{};
// };

// // @bench_meta {"id":"CaptureFixture/CaptureEncodeFrame","ru":"Кодирование кадра в видео","group":"Рендер/Захват"}
// BENCHMARK_DEFINE_F(CaptureFixture, CaptureEncodeFrame)(benchmark::State& state) {
//     if (!frameRecorder_ || !frameRecorder_->isRecording()) {
//         state.SkipWithError("Frame recorder is not available");
//         return;
//     }

//     for (auto _ : state) {
//         CapturedFrame frame = capturedFrame_;
//         const bool ok = frameRecorder_->submit(frame);
//         if (!ok) {
//             state.SkipWithError("Frame recorder submit failed");
//             break;
//         }
//         benchmark::ClobberMemory();
//     }
//     state.SetItemsProcessed(state.iterations());
// }

// BENCHMARK_REGISTER_F(CaptureFixture, CaptureEncodeFrame)->RangeMultiplier(8)->Range(125, 8000);
