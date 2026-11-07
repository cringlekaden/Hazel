#pragma once
#include "Hazel/Core/Base.h"

namespace Hazel {

    class GraphicsContext
    {
    public:
        virtual ~GraphicsContext() = default;

        static Scope<GraphicsContext> Create(void* window);
        static void ConfigureWindowHints();

        virtual void Init() = 0;
        virtual void SwapBuffers() = 0;
    };
}