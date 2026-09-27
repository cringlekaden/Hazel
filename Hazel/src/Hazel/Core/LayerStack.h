#pragma once

#include "Hazel/Core/Core.h"
#include "Hazel/Core/Layer.h"

#include <vector>

namespace Hazel
{
    class LayerStack
    {
    public:
        LayerStack();
        ~LayerStack();

        void PushLayer(Scope<Layer> layer);
        void PushOverlay(Scope<Layer> overlay);

        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        std::vector<Scope<Layer>>::iterator begin()
        {
            return m_Layers.begin();
        }

        std::vector<Scope<Layer>>::iterator end()
        {
            return m_Layers.end();
        }

    private:
        std::vector<Scope<Layer>> m_Layers;
        unsigned int m_LayerInsertIndex = 0;
    };
}