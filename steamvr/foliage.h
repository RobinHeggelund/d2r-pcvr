#pragma once
#include "foliage_shader.h"
namespace D2RL {struct PluginContext;}
namespace framevr::foliage {
void InstallCapacity(const D2RL::PluginContext* context,const wchar_t* ini,void (*logger)(const char*));
unsigned CapacityAxis();
void Register(void (*logger)(const char*),bool (*enabled)());
void Configure(Parameters parameters);
}
