#include "hzpch.h"
#include "OpenGLGPUTimer.h"
#include <glad/glad.h>
namespace Hazel
{
OpenGLGPUTimer::OpenGLGPUTimer()
{
    if (!glQueryCounter)
        return;
    GLint bits = 0;
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
    if (bits <= 0)
        return;
    if (bits < 64)
        m_CounterMask = (uint64_t(1) << bits) - 1;
    for (auto &s : m_Slots)
    {
        glGenQueries(1, &s.Start);
        glGenQueries(1, &s.End);
    }
}
OpenGLGPUTimer::~OpenGLGPUTimer()
{
    for (auto &s : m_Slots)
    {
        glDeleteQueries(1, &s.Start);
        glDeleteQueries(1, &s.End);
    }
}
void OpenGLGPUTimer::Begin(uint64_t submission)
{
    if (!m_Slots.front().Start)
        return;
    // Only read results after availability says ready. Timestamp pairs avoid
    // nesting a GL_TIME_ELAPSED query owned by a host/profiler.
    for (auto &s : m_Slots)
        if (s.Pending)
        {
            GLint ready = 0;
            glGetQueryObjectiv(s.End, GL_QUERY_RESULT_AVAILABLE, &ready);
            if (!ready)
                continue;
            GLuint64 a = 0, b = 0;
            glGetQueryObjectui64v(s.Start, GL_QUERY_RESULT, &a);
            glGetQueryObjectui64v(s.End, GL_QUERY_RESULT, &b);
            if (!m_Result.Available || s.Submission > m_Result.Submission)
                m_Result = {double((b - a) & m_CounterMask) / 1e6, s.Submission, true};
            s.Pending = false;
        }
    auto &s = m_Slots[m_Next];
    m_Active = !s.Pending;
    if (m_Active)
    {
        s.Submission = submission;
        glQueryCounter(s.Start, GL_TIMESTAMP);
    }
}
void OpenGLGPUTimer::End()
{
    if (!m_Active)
        return;
    auto &s = m_Slots[m_Next];
    glQueryCounter(s.End, GL_TIMESTAMP);
    s.Pending = true;
    m_Next = (m_Next + 1) % m_Slots.size();
    m_Active = false;
}
} // namespace Hazel
