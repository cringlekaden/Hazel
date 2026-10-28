#pragma once
#include "Hazel/Scripting/ScriptField.h"
#include <cstdint>
#include <functional>
#include <string_view>

namespace Hazel {
class Entity;
// Authored cross-entity fields only. Relationships have their own graph policy;
// runtime handles, events, captures and resources never participate. The visitor
// is synchronous; labels/references may not be retained. Callers own remap,
// optional/required and external-reference policy, and stage mutations as needed.
void VisitAuthoredEntityReferences(Entity entity, ScriptFieldMap& fields,
    const std::function<void(uint64_t&, std::string_view)>& visit);
}
