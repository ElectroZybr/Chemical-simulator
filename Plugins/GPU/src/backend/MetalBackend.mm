#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <QuartzCore/CAMetalLayer.h>

void* getMetalBackend(GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);

    NSView* view = [nsWindow contentView];

    CAMetalLayer* layer = [CAMetalLayer layer];

    [view setWantsLayer:YES];
    [view setLayer:layer];

    return (__bridge void*)layer;
}