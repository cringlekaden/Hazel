#include "MetalSurface.h"
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <stdexcept>
#include <algorithm>

namespace Hazel {
CA::MetalLayer* AttachMetalLayer(GLFWwindow* window, MTL::Device* device) {
    @autoreleasepool {
        if (![NSThread isMainThread]) throw std::logic_error("Metal surfaces require the main thread");
        NSWindow* native=glfwGetCocoaWindow(window);
        if (!native || !native.contentView) throw std::runtime_error("GLFW Cocoa content view unavailable");
        CAMetalLayer* layer=[CAMetalLayer layer];
        layer.device=(__bridge id<MTLDevice>)(void*)device;
        layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly=YES;
        layer.maximumDrawableCount=3;
        layer.allowsNextDrawableTimeout=YES;
        layer.wantsExtendedDynamicRangeContent=NO;
        CGColorSpaceRef color=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        layer.colorspace=color; CGColorSpaceRelease(color);
        native.contentView.wantsLayer=YES;
        native.contentView.layer=layer;
        // Transfer exactly one retain to MetalContext's C++ SharedPtr.
        return reinterpret_cast<CA::MetalLayer*>((__bridge_retained void*)layer);
    }
}
void DetachMetalLayer(GLFWwindow* window, CA::MetalLayer* layer) {
    @autoreleasepool {
        NSWindow* native=glfwGetCocoaWindow(window);
        NSView* view=native.contentView;
        if (view.layer==(__bridge CAMetalLayer*)(void*)layer) { view.layer=nil; view.wantsLayer=NO; }
    }
}
MetalSurfaceSize UpdateMetalLayer(GLFWwindow* window, CA::MetalLayer* metalLayer) {
    @autoreleasepool {
        NSWindow* native=glfwGetCocoaWindow(window);
        CAMetalLayer* layer=(__bridge CAMetalLayer*)(void*)metalLayer;
        int w=0,h=0,lw=0,lh=0;
        glfwGetFramebufferSize(window,&w,&h); glfwGetWindowSize(window,&lw,&lh);
        layer.contentsScale=native.backingScaleFactor;
        layer.frame=native.contentView.bounds;
        layer.drawableSize=CGSizeMake(w,h);
        return {uint32_t(std::max(w,0)),uint32_t(std::max(h,0)),uint32_t(std::max(lw,0)),uint32_t(std::max(lh,0)),
            double(native.backingScaleFactor), !glfwGetWindowAttrib(window,GLFW_ICONIFIED) && glfwGetWindowAttrib(window,GLFW_VISIBLE) &&
            bool(native.occlusionState & NSWindowOcclusionStateVisible)};
    }
}
}
