#pragma once
#include "foliage_shader.h"
namespace framevr::foliage {
void Register(void (*logger)(const char*),bool (*enabled)());
void Configure(Parameters parameters);
}
