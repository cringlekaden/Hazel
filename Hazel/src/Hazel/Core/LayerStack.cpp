#include "hzpch.h"

#include "Hazel/Core/LayerStack.h"

#include <algorithm>
#include <utility>

namespace Hazel {

    LayerStack::LayerStack() = default;

    LayerStack::~LayerStack()
    {
        for (auto& layer : m_Layers)
            layer->OnDetach();
    }

    void LayerStack::PushLayer(Scope<Layer> layer)
    {
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, std::move(layer));
        ++m_LayerInsertIndex;
        m_Layers[m_LayerInsertIndex - 1]->OnAttach();
    }

    void LayerStack::PushOverlay(Scope<Layer> overlay)
    {
        m_Layers.emplace_back(std::move(overlay));
        m_Layers.back()->OnAttach();
    }

    void LayerStack::PopLayer(Layer* layer)
    {
        auto boundary = m_Layers.begin() + m_LayerInsertIndex;
        auto it = std::find_if(m_Layers.begin(), boundary, [layer](const Scope<Layer>& current)
            {
                return current.get() == layer;
            });
        if(it != boundary)
        {
            (*it)->OnDetach();
            m_Layers.erase(it);  // Scope destroys the layer.
            --m_LayerInsertIndex;
        }
    }

    void LayerStack::PopOverlay(Layer* overlay)
    {
        auto it = std::find_if(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), [overlay](const Scope<Layer>& current)
            {
                return current.get() == overlay;
            });
        if(it != m_Layers.end())
        {
            (*it)->OnDetach();
            m_Layers.erase(it);
        }
    }
}