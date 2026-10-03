#pragma once
extern "C" { typedef struct _MonoImage MonoImage; }

namespace Hazel {

	class ScriptGlue
	{
	public:
		static void RegisterComponents();
		static void ValidateComponents(MonoImage* image);
		static void RegisterFunctions();
	};

}
