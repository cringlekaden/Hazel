#include "hzpch.h"
#include "MeshGeometry.h"
namespace Hazel {
MeshGeometry::MeshGeometry(const GeometryData& data){
    data.Validate();
    std::vector<float> packed;packed.reserve(data.Vertices.size()*16);
    for(auto& v:data.Vertices){for(int i=0;i<3;++i)packed.push_back(v.Position[i]);for(int i=0;i<3;++i)packed.push_back(v.Normal[i]);for(int i=0;i<4;++i)packed.push_back(v.Tangent[i]);for(int i=0;i<2;++i)packed.push_back(v.UV[i]);for(int i=0;i<4;++i)packed.push_back(v.Color[i]);}
    m_Vertices=VertexArray::Create();
    auto buffer=VertexBuffer::Create(packed.data(),uint32_t(packed.size()*sizeof(float)));
    buffer->SetLayout({{ShaderDataType::Float3,"Position"},{ShaderDataType::Float3,"Normal"},{ShaderDataType::Float4,"Tangent"},{ShaderDataType::Float2,"UV0"},{ShaderDataType::Float4,"Color0"}});
    m_Vertices->AddVertexBuffer(buffer);
    auto indices=data.Indices;m_Vertices->SetIndexBuffer(IndexBuffer::Create(indices.data(),uint32_t(indices.size())));
}
}
