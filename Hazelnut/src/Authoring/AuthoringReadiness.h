#pragma once
#include <string>
namespace Hazel {
    struct AuthoringReadinessInput {
        bool Project = false, Saved = false, ToolSDK = false, ToolsChecked = false,
             AssemblyLoaded = false, AssignedScripts = false, ClassesAvailable = false;
    };
    struct AuthoringReadiness {
        bool Editing = false, Scripts = false, Playing = false, Exporting = false;
        std::string EditReason, ScriptReason, PlayReason, ExportReason;
        static AuthoringReadiness Evaluate(const AuthoringReadinessInput &in) {
            AuthoringReadiness r;
            r.Editing = in.Project;
            r.EditReason = in.Project ? "Ready: native content editing and saving"
                                      : "Open or create a project";
            r.Scripts = in.Project && in.ToolSDK && in.ToolsChecked;
            r.ScriptReason = !in.Project   ? "Open a project"
                             : !in.ToolSDK ? "Select a compatible prepared Hazel source SDK"
                             : !in.ToolsChecked
                                 ? "Check script tools in Project > Build Scripts"
                                 : "Ready to build; reload requires a valid compiled assembly";
            r.Playing =
                in.Project && (!in.AssignedScripts || (in.AssemblyLoaded && in.ClassesAvailable));
            r.PlayReason = !in.Project           ? "Open a project"
                           : !in.AssignedScripts ? "Script-free Play needs no SDK or assembly"
                           : !in.AssemblyLoaded  ? "Build/reload the intended assembly first"
                           : !in.ClassesAvailable
                               ? "Assigned script classes unavailable; fix bindings/rebuild"
                               : "Assigned scripts ready; scene/resource validation still runs";
            r.Exporting = in.Project && in.Saved && in.ToolSDK && in.ToolsChecked;
            r.ExportReason = !in.Project        ? "Open a project"
                             : !in.Saved        ? "Save a descriptor/startup scene first"
                             : !in.ToolSDK      ? "Export needs a compatible source SDK"
                             : !in.ToolsChecked ? "Check export tools; compilation and dependency "
                                                  "validation are required"
                                                : "Prerequisites checked; export rebuilds selected "
                                                  "saved content and validates "
                                                  "closure";
            return r;
        }
    };
} // namespace Hazel
