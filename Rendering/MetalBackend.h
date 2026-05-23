#ifndef LATTICELAB_METALBACKEND_H
#define LATTICELAB_METALBACKEND_H

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#import <QuartzCore/CAMetalLayer.h>

void*  getMetalBackend(GLFWwindow* window);

#endif
