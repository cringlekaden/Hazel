#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
namespace Hazel
{
enum class DocumentLoadState { Ready, EditableWithProblems, NeedsDecision, Rejected };
struct DocumentProblem
{
    uint64_t Entity = 0;
    std::string Property, Message;
    std::filesystem::path Path;
    bool ChangesOnSave = false;
};
struct DocumentLoadReport
{
    DocumentLoadState State = DocumentLoadState::Rejected;
    bool Migration = false;
    std::vector<DocumentProblem> Problems;
    std::string Error;
    void Saved()
    {
        Migration=false;
        Problems.erase(std::remove_if(Problems.begin(),Problems.end(),[](const auto& problem){return problem.ChangesOnSave;}),Problems.end());
        if(State==DocumentLoadState::Ready || State==DocumentLoadState::EditableWithProblems)
            State=Problems.empty()?DocumentLoadState::Ready:DocumentLoadState::EditableWithProblems;
    }
};
}
