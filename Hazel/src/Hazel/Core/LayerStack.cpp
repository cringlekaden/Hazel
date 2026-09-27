#include "hzpch.h"
#include "Hazel/Core/LayerStack.h"

namespace Hazel {
    
    LayerStack::LayerStack()
    {
    }

    LayerStack::~LayerStack()
    {
        for (auto& layer : m_Layers)
        {
            layer->OnDetach();
        }
    }

    void LayerStack::PushLayer(Scope<Layer> layer)
    {
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, std::move(layer));
        ++m_LayerInsertIndex;
        layer->OnAttach();
    }

    void LayerStack::PushOverlay(Scope<Layer> overlay)
    {
        m_Layers.emplace_back(std::move(overlay));
        overlay->OnAttach();
    }

    void LayerStack::PopLayer(Scope<Layer> layer)
    {
        auto boundary = m_Layers.begin() + m_LayerInsertIndex;
        auto it = std::find(m_Layers.begin(), boundary, layer);
        if (it != boundary)
        {
            layer->OnDetach();
            m_Layers.erase(it);
            --m_LayerInsertIndex;
        }
    }

    void LayerStack::PopOverlay(Scope<Layer> overlay)
    {
        auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
        if (it != m_Layers.end())
        {
            overlay->OnDetach();
            m_Layers.erase(it);
        }
    }
}