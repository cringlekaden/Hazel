#pragma once
#include "DocumentHistory.h"
#include "Hazel/Scene/Scene.h"
#include <functional>
namespace Hazel
{
// Codec records/guards only. A candidate Edit scene is never retained in history.
class StructuralCommand final : public DocumentCommand
{
  public:
    StructuralCommand(const Ref<Scene> &before, const Ref<Scene> &after, std::string name,
                      uint64_t beforeSelection, uint64_t afterSelection);
    const std::string &Name() const override
    {
        return m_Name;
    }
    size_t Bytes() const override;
    bool Changed() const override
    {
        return !m_Records.empty();
    }
    void Check(Scene &) const override;
    void Apply(Scene &, bool undo) override;
    void Cancel(Scene &) override;
    void Reconcile(const std::vector<HistoryRestore> &) noexcept override;
    const std::vector<HistoryRestore> &Restorations() const override
    {
        return m_Restores;
    }
    std::optional<uint64_t> Selection(bool undo) const override
    {
        return undo ? m_BeforeSelection : m_AfterSelection;
    }

  private:
    struct Record
    {
        uint64_t ID;
        std::string Key, Before, After;
    }; // empty key = complete entity; empty value = absence
    // Incomplete asset-picker modes are authored in memory but intentionally
    // serialize as None. Keep only their existing typed, resource-free values.
    struct PickerImage
    {
        uint64_t ID;
        int Slot; // -1 Sprite, 0..4 Button state
        std::optional<SpriteSource> Before, After;
    };
    std::vector<PickerImage> m_PickerImages;
    struct ComponentValue
    {
        uint64_t ID;
        std::type_index Type;
        std::string Before, After; // existing exact authored inventory, no resources
    };
    std::vector<ComponentValue> m_Components;
    struct Boundary
    {
        uint64_t ID, Before, After;
    }; // external ancestor lifetimes
    std::vector<Boundary> m_Boundaries;
    struct Guards
    {
        HistoryStamp Before, After;
    };
    void CheckSide(Scene &, bool after) const;
    uint64_t m_Document, m_BeforeSelection, m_AfterSelection;
    std::string m_Name;
    std::vector<Record> m_Records;
    std::map<uint64_t, Guards> m_Guards;
    std::vector<HistoryRestore> m_Restores;
    bool m_Applied = false;
};
} // namespace Hazel
