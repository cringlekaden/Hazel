// Adapted upstream Application: Scope ownership and detach before renderer/context teardown.
#pragma once

#include "Hazel/Core/Base.h"
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "Hazel/Core/Window.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RendererCapabilities.h"
#include "Hazel/Core/LayerStack.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/ApplicationEvent.h"

#include "Hazel/Core/Timestep.h"

#include "Hazel/ImGui/ImGuiLayer.h"



namespace Hazel {

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			if (index < 0 || index >= Count) throw std::out_of_range("Application argument index");
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Hazel Application";
		bool EnableImGui = true;
        bool WindowVSync = true; // Submitted interval, not measured display timing.
        ApplicationResourceSpecification Resources;
		ApplicationCommandLineArgs CommandLineArgs;
        RendererSettings Rendering;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& specification = {});
        void Run();
		virtual ~Application();

		void OnEvent(Event& e);

		void PushLayer(Scope<Layer> layer);
		void PushOverlay(Scope<Layer> layer);

		Window& GetWindow() { return *m_Window; }

		void Close();
        // Main-thread service polling continues while the window is minimized.
        // Owner clears the callback before detaching; Application clears it before teardown.
        void SetBackgroundTick(std::function<void()> callback) { m_BackgroundTick=std::move(callback); }
        void SetCloseRequest(std::function<void()> callback) { m_CloseRequest=std::move(callback); }

		// Borrowed until Application shutdown; null when EnableImGui is false.
        ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		static Application& Get() { return *s_Instance; }
		static Application* TryGet() { return s_Instance; }

		const ApplicationSpecification& GetSpecification() const { return m_Specification; }

		void SubmitToMainThread(const std::function<void()>& function);
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		void ExecuteMainThreadQueue();
		void ShutdownResources();
	private:
		ApplicationSpecification m_Specification;
		Scope<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer = nullptr;
		bool m_Running = true;
        std::function<void()> m_CloseRequest, m_BackgroundTick;
		bool m_Minimized = false;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

		std::vector<std::function<void()>> m_MainThreadQueue;
		std::mutex m_MainThreadQueueMutex;
	private:
		static Application* s_Instance;

	};

	// To be defined in CLIENT
	Scope<Application> CreateApplication(ApplicationCommandLineArgs args);

}
