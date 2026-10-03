// Tool-only managed dependency validation. Never loaded into an application domain.
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Linq;

class PackageAudit {
    static readonly Dictionary<string, string> Files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
    static readonly HashSet<string> Visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    static void Audit(string path) {
        Assembly assembly = Assembly.ReflectionOnlyLoadFrom(path);
        string name = assembly.GetName().Name;
        if (!Visited.Add(name)) return;
        foreach (AssemblyName dependency in assembly.GetReferencedAssemblies()) {
            string target;
            if (Files.TryGetValue(dependency.Name, out target)) {
                if (AssemblyName.GetAssemblyName(target).FullName != dependency.FullName)
                    throw new InvalidOperationException("Managed dependency identity mismatch: " + dependency.FullName + " at " + target);
                Audit(target);
            } else {
                // Standard runtime references resolve through the selected SDK
                // runtime profile. These same assemblies are shipped in mono/lib.
                Assembly system = Assembly.ReflectionOnlyLoad(dependency.FullName);
                if (!system.Location.StartsWith(Path.GetDirectoryName(typeof(object).Assembly.Location), StringComparison.OrdinalIgnoreCase) && !system.GlobalAssemblyCache)
                    throw new InvalidOperationException("Unowned managed dependency: " + dependency.FullName);
            }
        }
        foreach (Type type in assembly.GetTypes()) {
            for (Type parent = type.BaseType; parent != null; parent = parent.BaseType)
                if (parent.FullName == "Hazel.Entity") { Console.WriteLine("SCRIPT " + type.FullName); break; }
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly))
                if ((method.Attributes & MethodAttributes.PinvokeImpl) != 0)
                    throw new InvalidOperationException("Project P/Invoke needs explicit native redistribution support before packaging: " + type.FullName + "." + method.Name);
        }
        Console.WriteLine("Validated managed assembly: " + name);
    }
    static int Main(string[] args) {
        try {
            if (args.Length != 3) throw new ArgumentException("PackageAudit Core.dll Project.dll AssetRoot");
            string core = Path.GetFullPath(args[0]);
            Files.Add(AssemblyName.GetAssemblyName(core).Name, core);
            foreach (string path in Directory.GetFiles(args[2], "*.dll", SearchOption.AllDirectories)) {
                string name = AssemblyName.GetAssemblyName(path).Name;
                string previous;
                if (Files.TryGetValue(name, out previous) && !File.ReadAllBytes(previous).SequenceEqual(File.ReadAllBytes(path)))
                    throw new InvalidOperationException("Conflicting managed assemblies: " + name);
                if (!Files.ContainsKey(name)) Files[name] = path;
            }
            AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve += (sender, request) => {
                string file;
                return Files.TryGetValue(new AssemblyName(request.Name).Name, out file) ? Assembly.ReflectionOnlyLoadFrom(file) : Assembly.ReflectionOnlyLoad(request.Name);
            };
            Audit(args[1]);
            return 0;
        } catch (Exception error) { Console.Error.WriteLine("Package validation failed: " + error.Message); return 1; }
    }
}
