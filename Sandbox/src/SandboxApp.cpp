#include <Hazel.h>

class Sandbox : public Hazel::Application
{
public:
    Sandbox()
    {

    }

    ~Sandbox()
    {

    }
};

int main()
{
    Sandbox* sandbox = new Sandbox();
    sandbox->Run();
    delete sandbox;
    return 0;
}