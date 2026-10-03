#include "hzpch.h"
#include "Project.h"

#include "ProjectSerializer.h"

namespace Hazel {

	std::filesystem::path Project::NormalizeAssetPath(const std::filesystem::path& path)
	{
		auto text = path.generic_u8string();
		std::replace(text.begin(), text.end(), '\\', '/');
		return std::filesystem::u8path(text);
	}

	std::filesystem::path Project::ResolveAssetPath(const std::filesystem::path& root, const std::filesystem::path& reference)
	{
		// Absolute external references keep their meaning; relative references are asset-root relative.
		const auto normalized = NormalizeAssetPath(reference);
#ifdef HZ_PLATFORM_LINUX
		const auto text = normalized.generic_u8string();
		if (text.size() >= 3 && text[1] == ':' && text[2] == '/')
			throw std::runtime_error("Windows absolute texture/asset reference needs an explicit local replacement: " + text);
#endif
		return (root / normalized).lexically_normal();
	}

	std::filesystem::path Project::MakeAssetReference(const std::filesystem::path& root, const std::filesystem::path& loadedPath)
	{
		const auto loaded = std::filesystem::absolute(loadedPath).lexically_normal();
		const auto base = std::filesystem::absolute(root).lexically_normal();
		const auto relative = loaded.lexically_relative(base);
		if (!relative.empty() && relative != "." && *relative.begin() != "..") return relative;
		// External textures remain absolute, including former cwd-relative external paths.
		return loaded;
	}

	Ref<Project> Project::New()
	{
		s_ActiveProject = CreateRef<Project>();
		return s_ActiveProject;
	}

	Ref<Project> Project::Load(const std::filesystem::path& path)
	{
		auto candidate = LoadCandidate(path);
		if (candidate) s_ActiveProject = candidate;
		return candidate;
	}

	Ref<Project> Project::LoadCandidate(const std::filesystem::path& path)
	{
		Ref<Project> project = CreateRef<Project>();

		ProjectSerializer serializer(project);
		if (serializer.Deserialize(path))
		{
			project->m_ProjectDirectory = path.parent_path();
			return project;
		}

		return nullptr;
	}

	bool Project::SaveActive(const std::filesystem::path& path)
	{
		if (!s_ActiveProject) return false;
		ProjectSerializer serializer(s_ActiveProject);
		if (serializer.Serialize(path))
		{
			s_ActiveProject->m_ProjectDirectory = path.parent_path();
			return true;
		}

		return false;
	}

}
