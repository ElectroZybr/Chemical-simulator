#include "World.h"

World::World(Vec3f size) : size(size), grid(size) { atomStorage_.reserve(250000); }
