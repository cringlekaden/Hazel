#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/object.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

extern "C" int HazelNativeAdd(int left, int right) { return left + right; }

static MonoAssembly* Open(MonoDomain* domain, const fs::path& path) {
    MonoAssembly* assembly = mono_domain_assembly_open(domain, path.string().c_str());
    if (!assembly) throw std::runtime_error("Could not load managed assembly: " + path.string());
    return assembly;
}

static std::string ExceptionText(MonoObject* exception) {
    if (!exception) return {};
    MonoObject* formattingFailure = nullptr;
    MonoString* text = mono_object_to_string(exception, &formattingFailure);
    if (formattingFailure || !text) return "managed exception (formatting failed)";
    char* utf8 = mono_string_to_utf8(text);
    std::string result = utf8 ? utf8 : "managed exception";
    mono_free(utf8);
    return result;
}

int main(int argc, char** argv) try {
    if (argc < 1) throw std::runtime_error("Missing executable path");
    const fs::path root = fs::canonical(fs::path(argv[0])).parent_path().parent_path();
    const fs::path sdk = root / "sdk";
    const fs::path fixturePath = root / "managed/MonoEmbeddingFixture.dll";
    const fs::path scriptCorePath = root / "managed/Hazel-ScriptCore.dll";
    const fs::path config = sdk / "etc/mono/config";

    mono_set_dirs((sdk / "lib").string().c_str(), (sdk / "etc").string().c_str());
    mono_config_parse(config.string().c_str());

    MonoDomain* rootDomain = mono_jit_init_version("HazelMacOSFeasibility", "v4.0.30319");
    if (!rootDomain) throw std::runtime_error("mono_jit_init_version failed");
    mono_add_internal_call("Hazel.MacOSFeasibility::NativeAdd",
                           reinterpret_cast<const void*>(&HazelNativeAdd));

    for (int iteration = 0; iteration < 4; ++iteration) {
        std::string name = "HazelReloadDomain" + std::to_string(iteration);
        MonoDomain* domain = mono_domain_create_appdomain(name.data(), nullptr);
        if (!domain || !mono_domain_set(domain, true))
            throw std::runtime_error("Could not create/set reload AppDomain");

        MonoAssembly* fixture = Open(domain, fixturePath);
        MonoImage* image = mono_assembly_get_image(fixture);
        MonoClass* type = mono_class_from_name(image, "Hazel", "MacOSFeasibility");
        MonoMethod* method = type ? mono_class_get_method_from_name(type, "Exercise", 1) : nullptr;
        if (!method) throw std::runtime_error("Managed fixture method not found");
        int seed = 40 + iteration;
        void* parameters[] = {&seed};
        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, nullptr, parameters, &exception);
        if (exception) throw std::runtime_error(ExceptionText(exception));
        if (!result || *static_cast<int*>(mono_object_unbox(result)) != seed + 5)
            throw std::runtime_error("Managed/internal-call result mismatch");

        MonoAssembly* scriptCore = Open(domain, scriptCorePath);
        MonoImage* scriptImage = mono_assembly_get_image(scriptCore);
        if (!mono_class_from_name(scriptImage, "Hazel", "Entity"))
            throw std::runtime_error("Actual Hazel ScriptCore Entity type not found");

        if (!mono_domain_set(rootDomain, false))
            throw std::runtime_error("Could not restore root AppDomain");
        mono_domain_unload(domain);
    }

    mono_jit_cleanup(rootDomain);
    std::cout << "PASS: arm64 Mono JIT, 4.7.2 compiler/profile, internal call, ScriptCore load, four AppDomain unload/reloads, cleanup\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
