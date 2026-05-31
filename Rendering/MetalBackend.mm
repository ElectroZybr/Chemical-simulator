#include "MetalBackend.h"

void* getMetalBackend(GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);

    NSView* view = [nsWindow contentView];

    CAMetalLayer* layer = [CAMetalLayer layer];

    [view setWantsLayer:YES];

    [view setLayer:layer];

    return (__bridge void*)layer;
}