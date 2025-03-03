#pragma once

#include <filesystem>
#include <boost/gil/typedefs.hpp>

#include "Types.h"

struct shape_metadata {
    boost::gil::point<long int> base_img_dim;
    boost::gil::point<int> coords;

    double deg;
    double sz_mul;
    pix_t col;
    int idx;
};

class Shaper {
    std::vector<alpha_img_t> templates;

public:
    float gen_boundaries_size_mul = 1.25;
    float mut_boundaries_base_img_mul = 0.2;

    explicit Shaper(const std::string &dir);

    shape_metadata mutateShapeData(const shape_metadata &md);

    alpha_img_t applyShapeData(const shape_metadata &md);

    shape_metadata generateShapeData(const alpha_img_t &base_img);
};
