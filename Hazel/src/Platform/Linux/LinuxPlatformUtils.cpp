// Native Linux implementations of upstream PlatformUtils; no renderer dependency.
#include "hzpch.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Core/Log.h"
#include <cstring>
#include <GLFW/glfw3.h>
#include <gtk/gtk.h>

namespace Hazel {
float Time::GetTime() { return static_cast<float>(glfwGetTime()); }

static std::string FileDialog(const char* filter, GtkFileChooserAction action)
{
    if (!gtk_init_check(nullptr, nullptr)) {
        HZ_CORE_ERROR("Unable to initialize native Linux file chooser");
        return {};
    }
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        action == GTK_FILE_CHOOSER_ACTION_OPEN ? "Open file" : "Save file", nullptr, action,
        "Cancel", GTK_RESPONSE_CANCEL, action == GTK_FILE_CHOOSER_ACTION_OPEN ? "Open" : "Save", GTK_RESPONSE_ACCEPT, nullptr);
    auto* chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    // Public filter convention: description\0pattern;pattern\0...\0, also used by Win32.
    const char* cursor = filter;
    std::string defaultExtension;
    while (cursor && *cursor) {
        auto* nativeFilter = gtk_file_filter_new();
        gtk_file_filter_set_name(nativeFilter, cursor);
        cursor += std::strlen(cursor) + 1;
        std::string patterns(cursor);
        std::size_t begin = 0;
        do {
            auto end = patterns.find(';', begin);
            auto pattern = patterns.substr(begin, end == std::string::npos ? end : end - begin);
            gtk_file_filter_add_pattern(nativeFilter, pattern.c_str());
            if (defaultExtension.empty() && pattern.rfind("*.", 0) == 0 && pattern.find('*', 1) == std::string::npos)
                defaultExtension = pattern.substr(1);
            if (end == std::string::npos) break;
            begin = end + 1;
        } while (begin < patterns.size());
        gtk_file_chooser_add_filter(chooser, nativeFilter);
        cursor += std::strlen(cursor) + 1;
    }
    std::string result;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(chooser);
        if (filename) { result = filename; g_free(filename); }
        if (action == GTK_FILE_CHOOSER_ACTION_SAVE && !defaultExtension.empty() && std::filesystem::path(result).extension().empty())
            result += defaultExtension;
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return result;
}
std::string FileDialogs::OpenFile(const char* filter) { return FileDialog(filter, GTK_FILE_CHOOSER_ACTION_OPEN); }
std::string FileDialogs::SaveFile(const char* filter) { return FileDialog(filter, GTK_FILE_CHOOSER_ACTION_SAVE); }
}
