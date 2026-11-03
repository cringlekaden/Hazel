#include "Hazel/Utils/PlatformUtils.h"
#import <Cocoa/Cocoa.h>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace Hazel {
static std::string Dialog(const char* filter, bool save, bool folder) {
    if (![NSThread isMainThread]) throw std::logic_error("Native dialogs require the main thread");
    @autoreleasepool {
        NSSavePanel* panel = save ? [NSSavePanel savePanel] : [NSOpenPanel openPanel];
        if (!save) { auto* open = (NSOpenPanel*)panel; open.canChooseDirectories=folder; open.canChooseFiles=!folder; open.allowsMultipleSelection=NO; }
        NSMutableArray<NSString*>* types=[NSMutableArray array];
        for (const char* cursor=filter; cursor && *cursor;) {
            cursor += std::strlen(cursor)+1;
            std::string patterns(cursor); size_t begin=0;
            do { auto end=patterns.find(';',begin); auto part=patterns.substr(begin,end-begin);
                if (part.rfind("*.",0)==0 && part.find('*',1)==std::string::npos)
                    [types addObject:[NSString stringWithUTF8String:part.substr(2).c_str()]];
                if(end==std::string::npos) break; begin=end+1;
            } while(begin<patterns.size());
            cursor += std::strlen(cursor)+1;
        }
        if(types.count && !folder) panel.allowedFileTypes=types;
        if([panel runModal]!=NSModalResponseOK) return {};
        return panel.URL.fileSystemRepresentation;
    }
}
std::string FileDialogs::OpenFile(const char* filter) { return Dialog(filter,false,false); }
std::string FileDialogs::SaveFile(const char* filter) { return Dialog(filter,true,false); }
std::string FileDialogs::SelectFolder() { return Dialog(nullptr,false,true); }
bool FileDialogs::OpenPath(const std::string& path) {
    @autoreleasepool { return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]]]; }
}
}
