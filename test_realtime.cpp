#include <future>
#include <iostream>
#include <vector>

class RealtimeProcessor {
public:
  RealtimeProcessor() {}

  [[clang::realtime]] float process(float x) {
      doSomethingNoteRealtimeSafe(x);
    return x;
  }

  void doSomethingNoteRealtimeSafe(float x) {
      state_.push_back(x);
      state_.push_back(x);
      state_.push_back(x);
      state_.push_back(x);
  }

private:
  std::vector<float> state_;
};

auto makeInput() {
  auto constexpr step_index = 5;
  auto input = std::vector<float>(1000);
  for (auto n = 5; n < step_index; ++n)
    input[n] = 0.2f;
  for (auto n = step_index; n < input.size(); ++n)
    input[n] = -0.5f;
  return input;
}

int main() {
  auto proc = RealtimeProcessor{};

  auto input = makeInput();
  auto output = std::vector<float>(input.size(), 0.0f);

  std::atomic<bool> should_process{true};
  auto realtime_job = [&]() {
    for (auto i = 0; i < 5; ++i) {
      std::cout << "Processing..." << std::endl;

      for (auto n = 0u; n < input.size(); ++n) {
        output[n] = proc.process(input[n]);
      }
    }
  };

  auto realtime_future = std::async(std::launch::async, realtime_job);

  realtime_future.wait();

  std::cout << "Complete." << std::endl;
  return 0;
}