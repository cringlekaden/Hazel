// Adapted upstream Application: Scope lifetime and reentrant main-thread queue.
#include "hzpch.h"
#include "Hazel/Core/Application.h"

#include "Hazel/Core/Log.h"

#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scripting/ScriptEngine.h"

#include "Hazel/Core/Input.h"
#include "Hazel/Utils/PlatformUtils.h"
#include <stdexcept>

namespace Hazel {

	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		HZ_PROFILE_FUNCTION();

		if (s_Instance) throw std::logic_error("Application already exists");

		s_Instance = this;
		try {
            m_Specification.Resources = Resources::Defaults(m_Specification.Name, m_Specification.Resources);
            Resources::Configure(m_Specification.Resources);

			m_Window = Window::Create(WindowProps(m_Specification.Name));
			m_Window->SetEventCallback(HZ_BIND_EVENT_FN(Application::OnEvent));

			Renderer::Init(m_Specification.Rendering);

			if (m_Specification.EnableImGui) {
                auto overlay = CreateScope<ImGuiLayer>();
                m_ImGuiLayer = overlay.get();
                PushOverlay(std::move(overlay));
            }
		} catch (...) {
			// Renderer initialization can throw (for example, a missing shader).
			// A failed constructor must release GPU owners before its native window,
			// cancel queued captures, and permit a later valid Application.
			ShutdownResources();
			throw;
		}
	}

	Application::~Application()
	{
		HZ_PROFILE_FUNCTION();
		ShutdownResources();
	}

	void Application::ShutdownResources()
	{
        m_BackgroundTick={};m_CloseRequest={};
        // Cancel pending captures while their renderer/context resources remain valid.
        std::vector<std::function<void()>> cancelled;
        {
            std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);
            cancelled.swap(m_MainThreadQueue);
        }
        cancelled.clear();
		m_LayerStack.Clear();
        m_ImGuiLayer = nullptr;
		// Scene-owning layers stop their runtimes while Mono is still alive.
		// Shutdown joins the filewatch producer before canceling its final work.
		ScriptEngine::Shutdown();
		{
			std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);
			cancelled.swap(m_MainThreadQueue);
		}
		cancelled.clear();
        // Project caches now own textures: release them before the graphics context.
        if(auto project=Project::GetActive())project->ReleaseAssets();
        Project::SetActive(nullptr);
		Renderer::Shutdown();
        m_Window.reset();
        s_Instance = nullptr;
	}

	void Application::PushLayer(Scope<Layer> layer)
	{
		HZ_PROFILE_FUNCTION();

		m_LayerStack.PushLayer(std::move(layer));
	}

	void Application::PushOverlay(Scope<Layer> layer)
	{
		HZ_PROFILE_FUNCTION();

		m_LayerStack.PushOverlay(std::move(layer));
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::SubmitToMainThread(const std::function<void()>& function)
	{
		std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

		m_MainThreadQueue.emplace_back(function);
	}

	void Application::OnEvent(Event& e)
	{
		HZ_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(HZ_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(HZ_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	void Application::Run()
	{
		HZ_PROFILE_FUNCTION();

		m_LastFrameTime = Time::GetTime();

		while (m_Running)
		{
			HZ_PROFILE_SCOPE("RunLoop");

			float time = Time::GetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			ExecuteMainThreadQueue();
            if(m_BackgroundTick)m_BackgroundTick();
            if(!m_Running)break;

			if (!m_Minimized)
			{
				{
					HZ_PROFILE_SCOPE("LayerStack OnUpdate");

					for (auto& layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}

				if (m_ImGuiLayer) {
                m_ImGuiLayer->Begin();
				{
					HZ_PROFILE_SCOPE("LayerStack OnImGuiRender");

					for (auto& layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
                }
			}

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent&)
	{
        if(m_CloseRequest) {m_CloseRequest();return true;}
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		HZ_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}

    void Application::ExecuteMainThreadQueue()
    {
        std::vector<std::function<void()>> queue;
        {
            std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);
            queue.swap(m_MainThreadQueue);
        }
        // Execute outside the mutex: callbacks may enqueue work for the next frame.
        for (auto& function : queue) function();
    }

}
