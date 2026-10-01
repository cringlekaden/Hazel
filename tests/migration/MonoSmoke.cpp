// Real native embedding: actual target managed API, reflection, GC and domains.
#include "Hazel/Core/Log.h"
#include "Hazel/Core/FileSystem.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/mono-config.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
static MonoAssembly* Load(const std::filesystem::path& path) {
    Hazel::ScopedBuffer bytes(Hazel::FileSystem::ReadFileBinary(path));
    Check(bytes.Data() && bytes.Size()<=std::numeric_limits<std::uint32_t>::max(),"Managed assembly file unavailable/oversized");
    MonoImageOpenStatus status{};
    auto image=mono_image_open_from_data_full(bytes.As<char>(),static_cast<std::uint32_t>(bytes.Size()),1,&status,0);
    Check(image && status==MONO_IMAGE_OK,"Managed image load failed");
    auto assembly=mono_assembly_load_from_full(image,path.generic_u8string().c_str(),&status,0);
    mono_image_close(image); Check(assembly && status==MONO_IMAGE_OK,"Managed assembly load failed"); return assembly;
}
struct Fixture {
    std::filesystem::path Path=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel-mono-é-"+std::to_string(std::random_device{}()));
    Fixture() { Check(std::filesystem::create_directory(Path),"Mono fixture isolation failed"); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(Path,error); }
};
int main(int argc,char** argv) {
    MonoDomain* root=nullptr;
    try {
        Hazel::Log::Init(); Check(argc==3,"Usage: MonoSmoke Core.dll Fixture.dll"); Fixture fixture;
        const auto unicode=fixture.Path/std::filesystem::u8path("Fixture-é.dll");
        std::filesystem::copy_file(std::filesystem::u8path(argv[2]),unicode);
        mono_set_assemblies_path(HZ_MONO_ASSEMBLIES_PATH);
        mono_config_parse(HZ_MONO_CONFIG_PATH[0] ? HZ_MONO_CONFIG_PATH : nullptr);
        root=mono_jit_init_version("HazelMigrationRoot","v4.0.30319"); Check(root,"Mono JIT initialization failed");
        mono_thread_set_main(mono_thread_current());
        for(int iteration=0;iteration<2;++iteration) {
            auto domain=mono_domain_create_appdomain(const_cast<char*>("HazelMigrationScripts"),nullptr); Check(domain,"Mono script domain creation failed");
            mono_domain_set(domain,true);
            auto core=Load(std::filesystem::u8path(argv[1])); auto app=Load(unicode);
            auto entity=mono_class_from_name(mono_assembly_get_image(core),"Hazel","Entity");
            auto probe=mono_class_from_name(mono_assembly_get_image(app),"Migration","Probe");
            Check(entity && probe && mono_class_is_subclass_of(probe,entity,false),"Actual Hazel managed entity API not loaded");
            auto object=mono_object_new(domain,probe); mono_runtime_object_init(object);
            const auto handle=mono_gchandle_new(object,true);
            float speed=0; std::uint16_t character=0; std::uint64_t number=0;
            mono_field_get_value(object,mono_class_get_field_from_name(probe,"Speed"),&speed);
            mono_field_get_value(object,mono_class_get_field_from_name(probe,"Character"),&character);
            mono_field_get_value(object,mono_class_get_field_from_name(probe,"Unsigned"),&number);
            Check(speed==2.5f && character==0xe9 && number==0xfedcba9876543210ULL,"Managed field reflection/layout mismatch");
            mono_gc_collect(mono_gc_max_generation()); object=mono_gchandle_get_target(handle); Check(object,"Managed owner lost after GC");
            float timestep=2; void* args[]{&timestep}; MonoObject* exception=nullptr;
            auto result=mono_runtime_invoke(mono_class_get_method_from_name(probe,"Evaluate",1),object,args,&exception);
            Check(result && !exception && *static_cast<float*>(mono_object_unbox(result))==21,"Managed Hazel vector/method invocation failed");
            exception=nullptr; mono_runtime_invoke(mono_class_get_method_from_name(probe,"ThrowManaged",0),nullptr,nullptr,&exception);
            Check(exception && std::strcmp(mono_class_get_name(mono_object_get_class(exception)),"InvalidOperationException")==0,
                  "Managed exception was not surfaced");
            mono_gchandle_free(handle); mono_domain_set(root,false); mono_domain_unload(domain);
        }
        mono_jit_cleanup(root); root=nullptr;
        std::cout<<"PASS: actual Hazel-ScriptCore/fixture assembly compilation and UTF-8 native loading, entity inheritance, scalar/vector reflection/invocation, GC handles, managed exceptions and repeated script domains\n"; return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        if(root) { mono_domain_set(root,false); mono_jit_cleanup(root); }
        return 1;
    }
}
