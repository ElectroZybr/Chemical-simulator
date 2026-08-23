#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

void* glfwMetalLayer(GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (!nsWindow)
        return nullptr;

    NSView* view = [nsWindow contentView];
    if (!view)
        return nullptr;

    CAMetalLayer* layer = [CAMetalLayer layer];
    [view setWantsLayer:YES];
    [view setLayer:layer];
    return (__bridge void*)layer;
}
