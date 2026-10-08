#pragma once

#include "Project.h"
#include "Hazel/Core/DocumentLoadReport.h"

namespace Hazel {

	class ProjectSerializer
	{
	public:
		ProjectSerializer(Ref<Project> project);

		bool Serialize(const std::filesystem::path& filepath);
        std::string SerializeText() const;
		bool Deserialize(const std::filesystem::path& filepath);
        const DocumentLoadReport& Report() const { return m_Report; }
	private:
		Ref<Project> m_Project;
        DocumentLoadReport m_Report;
	};

}
