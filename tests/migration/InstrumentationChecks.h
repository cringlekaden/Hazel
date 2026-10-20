static void InstrumentationChecks(const std::filesystem::path &root)
{
    using namespace Hazel;
    auto &trace = Instrumentor::Get();
    trace.EndSession();
    auto first = root / "trace-first.json", second = root / "trace-second.json";
    InstrumentationTimer inactive("Inactive scope must not join a subsequently opened session");
    trace.BeginSession("first", first.string());
    auto retired =
        CreateScope<InstrumentationTimer>("Retired scope must not enter replacement session");
    trace.BeginSession("second", second.string());
    {
        InstrumentationTimer current("Current scope with a quote: \"");
    }
    retired.reset();
    inactive.Stop();
    trace.EndSession();
    std::ifstream in(second);
    std::string data((std::istreambuf_iterator<char>(in)), {});
    Check(data.find("Current scope with a quote: \\\"") != std::string::npos &&
              data.find("Inactive scope") == std::string::npos &&
              data.find("Retired scope") == std::string::npos,
          "Trace scopes crossed session replacement or lost escaping");
    std::cout << "PASS: inactive tracing and scope retirement across trace-session replacement\n";
}
