#pragma once
#include "Hazel/Assets/Geometry.h"
#include "Hazel/Renderer/VertexArray.h"
namespace Hazel
{
// Renderer realization of one accepted CPU mesh. No importer or authored state.
class MeshGeometry
{
  public:
    explicit MeshGeometry(const GeometryData &data);
    const Ref<VertexArray> &Vertices() const
    {
        return m_Vertices;
    }

  private:
    Ref<VertexArray> m_Vertices;
};
} // namespace Hazel
