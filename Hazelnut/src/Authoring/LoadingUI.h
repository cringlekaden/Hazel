#pragma once
#include <chrono>
#include <functional>
#include <string>
namespace Hazel {
class LoadingUI {
public:
  bool Run(const std::string &operation, const std::function<bool()> &work);
  bool Busy() const { return m_Running || m_Queued; }
  bool Running() const { return m_Running; }
  void AfterUI(std::function<bool()> work);

private:
  bool m_Running = false, m_Queued = false;
  std::string m_Operation, m_Stage;
  std::chrono::steady_clock::time_point m_Start, m_StageStart, m_Presented;
  size_t m_Done = 0, m_Total = 0;
  unsigned m_Frames = 0;
  void Report(const std::string &stage, size_t done, size_t total);
};
} // namespace Hazel
