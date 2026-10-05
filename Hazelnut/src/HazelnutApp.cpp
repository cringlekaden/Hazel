#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>

#include "EditorLayer.h"
#include "Authoring/ConsoleModel.h"

namespace Hazel {

	class Hazelnut : private ConsoleSession, public Application
	{
	public:
		Hazelnut(const ApplicationSpecification& spec)
			: Application(spec)
		{
			PushLayer(CreateScope<EditorLayer>(Model));
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
