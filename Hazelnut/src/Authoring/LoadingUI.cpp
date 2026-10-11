#include "LoadingUI.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/OperationProgress.h"
#include <imgui.h>
namespace Hazel {
void LoadingUI::AfterUI(std::function<bool()> work) {
  if (m_Queued)
    return;
  m_Queued = true;
  Application::Get().SubmitToMainThread([this, work = std::move(work)] {
    m_Queued = false;
    work();
  });
}
bool LoadingUI::Run(const std::string &operation,
                    const std::function<bool()> &work) {
  if (m_Running)
    return work();
  // Direct service/test callers inside a frame retain their synchronous
  // contract. User document actions are dispatched by Guard after that frame.
  if (Application::Get().IsRenderingUI())
    return work();
  m_Running = true;
  m_Operation = operation;
  m_Stage.clear();
  m_Frames = 0;
  m_Start = m_StageStart = std::chrono::steady_clock::now();
  m_Presented = {};
  OperationProgress::Scope progress(
      [this](const auto &stage, size_t done, size_t total) {
        Report(stage, done, total);
      });
  auto finish = [&](bool success) {
    Report(success ? "Ready" : "Operation failed — see Console", 1, 1);
    HZ_CORE_INFO("Authoring load '{}' finished in {:.3f}s; {} progress frames",
                 operation,
                 std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - m_Start)
                     .count(),
                 m_Frames);
    m_Running = false;
  };
  try {
    Report("Preparing workspace", 0, 0);
    const bool result = work();
    finish(result);
    return result;
  } catch (...) {
    finish(false);
    throw;
  }
}
void LoadingUI::Report(const std::string &stage, size_t done, size_t total) {
  const auto now = std::chrono::steady_clock::now();
  bool changed = stage != m_Stage;
  if (changed) {
    if (!m_Stage.empty())
      HZ_CORE_INFO("Authoring phase '{}' {:.3f}s", m_Stage,
                   std::chrono::duration<double>(now - m_StageStart).count());
    m_StageStart = now;
    m_Stage = stage;
  }
  m_Done = done;
  m_Total = total;
  if (!changed && now - m_Presented < std::chrono::milliseconds(33))
    return;
  m_Presented = now;
  ++m_Frames;
  Application::Get().PresentProgress([&] {
    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("Loading workspace", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("Hazelnut");
    ImGui::Separator();
    ImGui::TextWrapped("%s", m_Operation.c_str());
    ImGui::Spacing();
    ImGui::TextUnformatted(m_Stage.c_str());
    if (m_Total) {
      ImGui::ProgressBar(float(m_Done) / float(m_Total), {0, 0});
      ImGui::Text("%zu of %zu", m_Done, m_Total);
    } else {
      const char *dots[] = {"Working.", "Working..", "Working..."};
      ImGui::TextUnformatted(dots[int(ImGui::GetTime() * 3) % 3]);
    }
    ImGui::TextDisabled("Elapsed %.1f seconds",
                        std::chrono::duration<double>(now - m_Start).count());
    ImGui::TextWrapped("The workspace is being checked. Editing resumes when "
                       "this operation finishes.");
    ImGui::End();
  });
}
} // namespace Hazel
