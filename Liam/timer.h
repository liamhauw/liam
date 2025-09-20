#ifndef TIMER_H
#define TIMER_H
#include "pch.h"
//

// Helper class for animation and simulation timing.
class Timer {
 public:
  Timer()
      : elapsed_ticks_(0),
        total_ticks_(0),
        left_over_ticks_(0),
        frame_count_(0),
        frames_per_second_(0),
        frames_this_second_(0),
        qpc_second_counter_(0),
        fixed_time_step_(false),
        target_elapsed_ticks_(kTicksPerSecond / 60) {
    QueryPerformanceFrequency(&qpc_frequency_);
    QueryPerformanceCounter(&qpc_last_time_);

    // Initialize max delta to 1/10 of a second.
    qpc_max_delta_ = qpc_frequency_.QuadPart / 10;
  }

  // Get elapsed time since the previous Update call.
  UINT64 elapsed_ticks() const { return elapsed_ticks_; }
  double CalcElapsedSeconds() const { return TicksToSeconds(elapsed_ticks_); }

  // Get total time since the start of the program.
  UINT64 total_ticks() const { return total_ticks_; }
  double CalcTotalSeconds() const { return TicksToSeconds(total_ticks_); }

  // Get total number of updates since start of the program.
  UINT32 frame_count() const { return frame_count_; }

  // Get the current framerate.
  UINT32 frames_per_second() const { return frames_per_second_; }

  // Set whether to use fixed or variable timestep mode.
  void set_fixed_time_step(bool fixed_time_step) {
    fixed_time_step_ = fixed_time_step;
  }

  // Set how often to call Update when in fixed timestep mode.
  void set_target_elapsed_ticks(UINT64 target_elapsed_ticks) {
    target_elapsed_ticks_ = target_elapsed_ticks;
  }
  void set_target_elapsed_ticks_(double target_elapsed_seconds) {
    target_elapsed_ticks_ = SecondsToTicks(target_elapsed_seconds);
  }

  // Integer format represents time using 10,000,000 ticks per second.
  static const UINT64 kTicksPerSecond = 10000000;

  static double TicksToSeconds(UINT64 ticks) {
    return static_cast<double>(ticks) / kTicksPerSecond;
  }
  static UINT64 SecondsToTicks(double seconds) {
    return static_cast<UINT64>(seconds * kTicksPerSecond);
  }

  // After an intentional timing discontinuity (for instance a blocking IO
  // operation) call this to avoid having the fixed timestep logic attempt a set
  // of catch-up Update calls.

  void ResetElapsedTime() {
    QueryPerformanceCounter(&qpc_last_time_);

    left_over_ticks_ = 0;
    frames_per_second_ = 0;
    frames_this_second_ = 0;
    qpc_second_counter_ = 0;
  }

  typedef void (*LPUPDATEFUNC)(void);

  // Update timer state, calling the specified Update function the appropriate
  // number of times.
  void Tick(LPUPDATEFUNC update = nullptr) {
    // Query the current time.
    LARGE_INTEGER current_time;

    QueryPerformanceCounter(&current_time);

    UINT64 time_delta = current_time.QuadPart - qpc_last_time_.QuadPart;

    qpc_last_time_ = current_time;
    qpc_second_counter_ += time_delta;

    // Clamp excessively large time deltas (e.g. after paused in the debugger).
    if (time_delta > qpc_max_delta_) {
      time_delta = qpc_max_delta_;
    }

    // Convert QPC units into a canonical tick format. This cannot overflow due
    // to the previous clamp.
    time_delta *= kTicksPerSecond;
    time_delta /= qpc_frequency_.QuadPart;

    UINT32 last_frame_count = frame_count_;

    if (fixed_time_step_) {
      // Fixed timestep update logic

      // If the app is running very close to the target elapsed time (within 1/4
      // of a millisecond) just clamp the clock to exactly match the target
      // value. This prevents tiny and irrelevant errors from accumulating over
      // time. Without this clamping, a game that requested a 60 fps fixed
      // update, running with vsync enabled on a 59.94 NTSC display, would
      // eventually accumulate enough tiny errors that it would drop a frame. It
      // is better to just round small deviations down to zero to leave things
      // running smoothly.

      if (abs(static_cast<int>(time_delta - target_elapsed_ticks_)) <
          kTicksPerSecond / 4000) {
        time_delta = target_elapsed_ticks_;
      }

      left_over_ticks_ += time_delta;

      while (left_over_ticks_ >= target_elapsed_ticks_) {
        elapsed_ticks_ = target_elapsed_ticks_;
        total_ticks_ += target_elapsed_ticks_;
        left_over_ticks_ -= target_elapsed_ticks_;
        frame_count_++;

        if (update) {
          update();
        }
      }
    } else {
      // Variable timestep update logic.
      elapsed_ticks_ = time_delta;
      total_ticks_ += time_delta;
      left_over_ticks_ = 0;
      frame_count_++;

      if (update) {
        update();
      }
    }

    // Track the current framerate.
    if (frame_count_ != last_frame_count) {
      frames_this_second_++;
    }

    if (qpc_second_counter_ >= static_cast<UINT64>(qpc_frequency_.QuadPart)) {
      frames_per_second_ = frames_this_second_;
      frames_this_second_ = 0;
      qpc_second_counter_ %= qpc_frequency_.QuadPart;
    }
  }

 private:
  // Source timing data uses QPC units.
  LARGE_INTEGER qpc_frequency_;
  LARGE_INTEGER qpc_last_time_;
  UINT64 qpc_max_delta_;

  // Derived timing data uses a canonical tick format.
  UINT64 elapsed_ticks_;
  UINT64 total_ticks_;
  UINT64 left_over_ticks_;

  // Members for tracking the framerate.
  UINT32 frame_count_;
  UINT32 frames_per_second_;
  UINT32 frames_this_second_;
  UINT64 qpc_second_counter_;

  // Members for configuring fixed timestep mode.
  bool fixed_time_step_;
  UINT64 target_elapsed_ticks_;
};

#endif  // !TIMER_H
