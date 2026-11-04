#pragma once
#include "foliage_shader.h"
#include "vr_settings.h"
namespace framevr::foliage {
inline Parameters FromSettings(const settings::Values& v){
    using namespace settings;
    return Normalize({v[FoliageFootprint],v[FoliageOriginX],v[FoliageOriginZ],v[FoliageSpacing],v[FoliageCamera],
                      v[FoliageHeightScale],v[FoliageHeightX],v[FoliageHeightZ]});
}
}
