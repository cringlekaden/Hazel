// Native environment and input qualification inside the actual smoke process.
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include <stdexcept>
#include "Hazel/Core/TextInput.h"

bool MetalM1ClipboardRoundTrip() {
    @autoreleasepool {
        NSPasteboard* board=[NSPasteboard generalPasteboard];
        NSMutableArray* saved=[NSMutableArray array];
        for(NSPasteboardItem* item in board.pasteboardItems) {
            NSPasteboardItem* copy=[[[NSPasteboardItem alloc] init] autorelease];
            for(NSString* type in item.types) {
                NSData* data=[item dataForType:type];
                if(!data)throw std::runtime_error("Cannot preserve the native clipboard representation");
                [copy setData:data forType:type];
            }
            [saved addObject:copy];
        }
        struct Restore {
            NSPasteboard* Board; NSArray* Items;
            bool Applied=false;
            bool Apply(){[Board clearContents];Applied=true;return !Items.count || [Board writeObjects:Items];}
            ~Restore(){if(!Applied)Apply();}
        } restore{board,saved};
        Hazel::NativeTextClipboard clipboard;
        std::string actual,error;
        const std::string expected="Hazel M5 native clipboard é";
        const bool result=clipboard.Write(expected,error) && clipboard.Read(actual,16384,error) && actual==expected;
        return restore.Apply() && result;
    }
}

void MetalM1Environment(GLFWwindow* window, const char* output) {
    @autoreleasepool {
        CFDictionaryRef raw=CGSessionCopyCurrentDictionary();
        if(!raw) throw std::runtime_error("Smoke process has no GUI console session");
        NSDictionary* session=(NSDictionary*)raw;
        NSData* sessionBytes=[NSJSONSerialization dataWithJSONObject:session options:NSJSONWritingPrettyPrinted error:nullptr];
        NSString* sessionPath=[[@(output) stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"process-session.json"];
        [sessionBytes writeToFile:sessionPath atomically:YES];
        bool interactive=[session[@"kCGSSessionOnConsoleKey"] boolValue] &&
            [session[@"kCGSessionLoginDoneKey"] boolValue] &&
            [session[@"kCGSSessionUserIDKey"] unsignedIntValue]==getuid() &&
            ![session[@"CGSSessionScreenIsLocked"] boolValue];
        if(!interactive) { CFRelease(raw); throw std::runtime_error("Smoke process is outside the intended unlocked GUI console session"); }
        NSWindow* native=glfwGetCocoaWindow(window);
        int lw=0,lh=0,pw=0,ph=0,count=0;
        float sx=0,sy=0;
        glfwGetWindowSize(window,&lw,&lh); glfwGetFramebufferSize(window,&pw,&ph);
        glfwGetWindowContentScale(window,&sx,&sy);
        GLFWmonitor** monitors=glfwGetMonitors(&count);
        if(!count) { CFRelease(raw); throw std::runtime_error("GLFW exposes no monitors in the unlocked smoke session"); }
        NSMutableArray* glfw=[NSMutableArray array];
        for(int i=0;i<count;++i) {
            const auto* mode=glfwGetVideoMode(monitors[i]); float x=0,y=0;glfwGetMonitorContentScale(monitors[i],&x,&y);
            [glfw addObject:@{@"name":@(glfwGetMonitorName(monitors[i])),@"logicalWidth":@(mode->width),@"logicalHeight":@(mode->height),@"refreshHz":@(mode->refreshRate),@"scaleX":@(x),@"scaleY":@(y)}];
        }
        NSMutableArray* cocoa=[NSMutableArray array];
        for(NSScreen* screen in NSScreen.screens) {
            [cocoa addObject:@{@"name":screen.localizedName,@"scale":@(screen.backingScaleFactor),@"logicalWidth":@(screen.frame.size.width),@"logicalHeight":@(screen.frame.size.height)}];
        }
        CFArrayRef list=CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow,(CGWindowID)native.windowNumber);
        bool listed=false;
        for(NSDictionary* info in (NSArray*)list) if([info[(NSString*)kCGWindowOwnerPID] intValue]==getpid()) listed=true;
        if(list) CFRelease(list);
        // Cocoa owns this window even before WindowServer publishes its list entry.
        bool owned=[NSApp.windows containsObject:native];
        if(!owned) { CFRelease(raw); throw std::runtime_error("Smoke window is not owned by this interactive process"); }
        NSDictionary* report=@{@"pid":@(getpid()),@"uid":@(getuid()),@"session":session,@"interactive":@YES,@"windowOwnedByNSApplication":@(owned),@"windowServerListsOwnerPID":@(listed),
            @"appHidden":@(NSApp.hidden),@"windowVisible":@(native.visible),@"windowMiniaturized":@(native.miniaturized),@"windowOcclusionState":@(native.occlusionState),@"windowNumber":@(native.windowNumber),@"logicalWidth":@(lw),@"logicalHeight":@(lh),@"framebufferWidth":@(pw),@"framebufferHeight":@(ph),
            @"glfwScaleX":@(sx),@"glfwScaleY":@(sy),@"cocoaBackingScale":@(native.backingScaleFactor),@"glfwMonitors":glfw,@"cocoaScreens":cocoa};
        NSError* error=nil;
        NSData* bytes=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:&error];
        bool saved=bytes && [bytes writeToFile:@(output) options:NSDataWritingAtomic error:&error];
        CFRelease(raw);
        if(!saved) throw std::runtime_error("Cannot write native smoke environment evidence");
        printf("M1_ENV pid=%d uid=%u console=unlocked window_owner=matched monitors=%d logical=%dx%d framebuffer=%dx%d glfw_scale=%.2fx%.2f cocoa_scale=%.2f\n",getpid(),getuid(),count,lw,lh,pw,ph,sx,sy,double(native.backingScaleFactor));
    }
}
