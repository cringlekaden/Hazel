#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Hazel {

	class Hazelnut : public Application
	{
	public:
		Hazelnut(const ApplicationSpecification& spec)
			: Application(spec)
		{
			PushLayer(CreateScope<EditorLayer>());
		}
	};

	Scope<Application> CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Hazelnut";
		spec.CommandLineArgs = args;

		return CreateScope<Hazelnut>(spec);
	}

}
