/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "open3d/geometry/Image.h"

open3d::geometry::Image createColormap(const open3d::geometry::Image &rgb,
                                       const open3d::geometry::Image &depth,
                                       float blend, float scale, float trunc);
