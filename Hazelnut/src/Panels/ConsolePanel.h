#pragma once
#include "Authoring/ConsoleModel.h"
#include <functional>
namespace Hazel
{
class ConsolePanel
{
  public:
    std::function<void()> DocumentProblems; // Main-thread presentation owned by the controller.
    explicit ConsolePanel(std::shared_ptr<ConsoleModel> model) : m_Model(std::move(model))
    {
    }
    void Render();
    void Show()
    {
        Visible = true;
        m_RevealRequested = true;
    } // Explicit user request, never a log/failure callback.
    bool Visible = false, ExitRequested = false, CancelExit = false, SaveCapture = false;
    bool CapturePreferenceAvailable = true;
    int CaptureLevel = spdlog::level::info;

  private:
    void RefreshRows(bool reset);
    std::shared_ptr<ConsoleModel> m_Model;
    ConsoleFilter m_Filter;
    std::vector<uint64_t> m_Visible, m_Selected;
    uint64_t m_LastSequence = 0, m_Detail = 0, m_Operation = 0;
    uint64_t m_Revision = UINT64_MAX;
    std::vector<ConsoleOperation> m_OperationCache;
    std::optional<ConsoleOperation> m_FailureCache;
    std::string m_CheckedArtifact;
    std::filesystem::path m_ArtifactFolder;
    bool m_FolderExists = false, m_RevealRequested = false;
    bool m_Follow = true, m_Jump = false, m_Operations = false;
};
} // namespace Hazel
