#pragma once

#include "Hazel/Core/Core.h"

#include <cstdint>
#include <string>

namespace Hazel {

    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;

        virtual void SetData(const void* data, std::uint32_t size) = 0;

        virtual void Bind(std::uint32_t slot = 0) const = 0;

        virtual bool operator==(const Texture& other) const = 0;
    };

    class Texture2D : public Texture
    {
    public:
        static Ref<Texture2D> Create(std::uint32_t width, std::uint32_t height);
        static Ref<Texture2D> Create(const std::string& path);
    };
}