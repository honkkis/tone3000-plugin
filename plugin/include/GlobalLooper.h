#pragma once
#include "MonoLooper.h"
#include <atomic>

// UI commands are published atomically. Only the audio thread touches the
// take/transport; prepare() runs while callbacks are stopped. Neither the
// UI nor preset-loading thread needs the chain lock to control this tool.
class GlobalLooper {
public:
  enum class Command : unsigned { record = 1, stop = 2, play = 3 };
  void prepare(double rate) { engine.prepare(rate); publish(); }
  void request(Command command) {
    unsigned previous = pending.load(std::memory_order_relaxed);
    unsigned next;
    do {
      // Preserve a pending Record's deletion even if Stop/Play arrives
      // before the next audio callback. Latest transport command wins.
      next = (previous & resetTake) | static_cast<unsigned>(command);
      if (command == Command::record) next |= resetTake;
    } while (!pending.compare_exchange_weak(previous, next, std::memory_order_release,
                                             std::memory_order_relaxed));
  }
  void setMix(float value) { mix.store(std::clamp(value, 0.0f, 1.0f)); }
  void setPan(float value) { pan.store(std::clamp(value, -1.0f, 1.0f)); }
  float getMix() const { return mix.load(); }
  float getPan() const { return pan.load(); }
  MonoLooper::State getState() const { return static_cast<MonoLooper::State>(state.load()); }
  double seconds() const { return duration.load(); }
  void process(float* left, float* right, int samples, bool stereoOutput = true) {
    const auto command = pending.exchange(0, std::memory_order_acquire);
    if (command & resetTake) engine.record();
    switch (command & transportMask) {
      case static_cast<unsigned>(Command::stop): engine.stop(); break;
      case static_cast<unsigned>(Command::play): engine.stop(); engine.play(); break;
      default: break;
    }
    engine.process(left, right, samples, mix.load(), true,
                   stereoOutput ? pan.load() : 0.0f, stereoOutput);
    publish();
  }
private:
  void publish() { state.store(static_cast<int>(engine.getState())); duration.store(engine.seconds()); }
  static constexpr unsigned resetTake = 4, transportMask = 3;
  MonoLooper engine;
  std::atomic<unsigned> pending{0};
  std::atomic<float> mix{0.5f}, pan{0};
  std::atomic<int> state{static_cast<int>(MonoLooper::State::stopped)};
  std::atomic<double> duration{0};
};
