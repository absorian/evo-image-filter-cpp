#pragma once

#include <filesystem>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/image.hpp>

#include "Types.h"

struct shape_metadata {
    boost::gil::point<int> coords;
    double deg;
    double sz_mul;
    int idx;
};

class Shaper {
    std::vector<alpha_img_t> templates;
    alpha_img_t base_img;
    struct {
        int xlow, xhigh, ylow, yhigh;
    } coords_bounds;

public:
    const float gen_boundaries_sz_mul = 1.25;
    const float mut_boundaries_base_img_mul = 0.2;
    const float mut_boundaries_shape_deg = 60;
    const float mut_boundaries_shape_sz_mul = 0.5;

    explicit Shaper(const std::string &dir, boost::gil::point<int> shape_sz);

    void setBaseImage(const alpha_img_t &img);

    shape_metadata mutateShapeData(const shape_metadata &md) const;

    alpha_img_t applyShapeData(const shape_metadata &md) const;

    shape_metadata generateShapeData() const;
};
