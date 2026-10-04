#include "hzpch.h"
#include "Project.h"

#include "ProjectSerializer.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Assets/ProjectAssets.h"

namespace Hazel {
    Ref<ProjectAssets> Project::GetAssets() const {
        auto root=std::filesystem::weakly_canonical(GetAssetRoot());
        if(!m_Assets || m_Assets->Root()!=root) m_Assets=CreateRef<ProjectAssets>(root);
        return m_Assets;
    }
	std::filesystem::path Project::ResolveOwnedAsset(const std::filesystem::path& root, const std::filesystem::path& reference)
	{
		auto path = NormalizeAssetPath(reference);
		const auto text = path.generic_u8string();
		if (path.empty() || path.has_root_path() || text.find(':') != std::string::npos || text.find('\0') != std::string::npos)
			throw std::runtime_error("Use a project asset-relative path: " + text);
		for (const auto& part : path) if (part == "..") throw std::runtime_error("Asset path cannot contain parent traversal: " + text);
		auto base = std::filesystem::weakly_canonical(root);
		auto resolved = std::filesystem::weakly_canonical(base / path);
		auto relative = resolved.lexically_relative(base);
		if (relative.empty() || relative == "." || *relative.begin() == "..") throw std::runtime_error("Asset/symlink escapes project Assets: " + text);
		return resolved;
	}

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
		const auto text = normalized.generic_u8string();
		if (text.size() >= 3 && text[1] == ':' && text[2] == '/' && !normalized.is_absolute())
			throw std::runtime_error("Windows absolute texture/asset reference needs an explicit local replacement: " + text);
		return (root / normalized).lexically_normal();
	}

	std::filesystem::path Project::MakeAssetReference(const std::filesystem::path& root, const std::filesystem::path& loadedPath)
	{
		// Resource caches and ownership checks use canonical roots. Match that
		// convention here too (Windows 8.3 aliases and existing symlinks included),
		// while weak canonicalization also supports not-yet-created destinations.
		const auto loaded = std::filesystem::weakly_canonical(std::filesystem::absolute(loadedPath));
		const auto base = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
		const auto relative = loaded.lexically_relative(base);
		if (!relative.empty() && relative != "." && *relative.begin() != "..") return relative;
		// External textures remain absolute, including former cwd-relative external paths.
		return loaded;
	}

    Ref<Scene> Project::LoadScene(const std::filesystem::path& reference) const {
        const auto root = GetAssetRoot();
        if (!std::filesystem::is_directory(root)) throw std::runtime_error("Missing project asset root: " + root.generic_u8string());
        const auto path = ResolveAssetPath(root, reference);
        if (path.extension() != ".hazel") throw std::runtime_error("Scene must be a .hazel file: " + path.generic_u8string());
        auto scene = CreateRef<Scene>();
        if (!SceneSerializer(scene, root, false, GetAssets()).Deserialize(path.generic_u8string()))
            throw std::runtime_error("Cannot load scene/assets: " + path.generic_u8string());
        return scene;
    }

    std::vector<std::filesystem::path> Project::Discover(const std::filesystem::path& directory) {
        std::vector<std::filesystem::path> paths;
        if (!std::filesystem::is_directory(directory)) return paths;
        for (const auto& item : std::filesystem::directory_iterator(directory))
            if (item.is_regular_file() && item.path().extension() == ".hproj") paths.push_back(item.path());
        std::sort(paths.begin(), paths.end());
        return paths;
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
			project->m_ProjectDirectory = std::filesystem::absolute(path).parent_path();
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
			s_ActiveProject->m_ProjectDirectory = std::filesystem::absolute(path).parent_path();
			return true;
		}

		return false;
	}

}
