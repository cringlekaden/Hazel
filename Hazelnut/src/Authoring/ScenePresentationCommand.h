#pragma once
#include "DocumentHistory.h"
#include "Hazel/Scene/Scene.h"
namespace Hazel {
class ScenePresentationCommand final : public DocumentCommand {
    uint64_t m_Document;ScenePresentation m_Before,m_After;bool m_Applied=false;
    std::string m_Name="Scene presentation";
public:
    ScenePresentationCommand(Scene& scene,ScenePresentation after):m_Document(scene.GetIdentity()),m_Before(scene.Presentation()),m_After(after){m_After.Validate();}
    const std::string& Name() const override{return m_Name;}
    size_t Bytes() const override{return sizeof(*this);}
    bool Changed() const override{return m_Before!=m_After;}
    void Check(Scene& scene) const override {
        if(scene.GetIdentity()!=m_Document || scene.Presentation()!=(m_Applied?m_After:m_Before))
            throw std::runtime_error("Scene presentation changed outside history; cancel/retry");
    }
    void Apply(Scene& scene,bool undo) override {Check(scene);scene.Presentation()=undo?m_Before:m_After;m_Applied=!undo;}
    void Cancel(Scene& scene) override {if(m_Applied)Apply(scene,true);}
};
}
