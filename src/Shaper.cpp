

#include "Shaper.h"

#include <iostream>
#include <boost/gil/image.hpp>

#include "Types.h"
#include "ImageOps.h"
#include "Util.h"

using namespace boost::gil;

Shaper::Shaper(const std::string &dir) {
    std::filesystem::path path{dir};

    for (const auto &entry: std::filesystem::directory_iterator{path}) {
        if (entry.is_directory()) continue;

        alpha_img_t img;
        try {
            img = read_png_or_jpg(entry.path().string());
        } catch (std::ios_base::failure &) {
            std::cerr << "Shaper: failed to read image \"" << entry.path().filename().string() << "\"";
        }

        templates.push_back(scale_image(grayscale_filter(img), {100, 100}));
    }
    if (templates.empty()) {
        throw std::runtime_error("Shaper got nothing from specified directory: " + path.string());
    }
}

void Shaper::setBaseImage(const alpha_img_t &img) {
    base_img = img;
    auto dim = base_img.dimensions();

    coords_bounds.xlow = -(gen_boundaries_size_mul - 1) * dim.x;
    coords_bounds.xhigh = dim.x * gen_boundaries_size_mul;
    coords_bounds.ylow = -(gen_boundaries_size_mul - 1) * dim.y;
    coords_bounds.yhigh = dim.y * gen_boundaries_size_mul;
}

shape_metadata Shaper::mutateShapeData(const shape_metadata &md) {
    const float base_img_mul = mut_boundaries_base_img_mul;
    const auto base_img_dim = base_img.dimensions();
    point coords{
        lrand(-base_img_mul * base_img_dim.x,
              base_img_mul * base_img_dim.x),
        lrand(-base_img_mul * base_img_dim.y,
              base_img_mul * base_img_dim.y),
    };
    coords += md.coords;

    if (coords.x < coords_bounds.xlow)
        coords.x = coords_bounds.xlow;
    else if (coords.x > coords_bounds.xhigh)
        coords.x = coords_bounds.xhigh;

    if (coords.y < coords_bounds.ylow)
        coords.y = coords_bounds.ylow;
    else if (coords.y > coords_bounds.yhigh)
        coords.y = coords_bounds.yhigh;

    return {
        coords,
        md.deg + drand(-60, 60),
        md.sz_mul * drand(0.5, 1.5),
        md.idx
    };
}

alpha_img_t Shaper::applyShapeData(const shape_metadata &md) {
    const auto &src_img = templates[md.idx];

    auto bview = const_view(base_img);
    auto sview = const_view(src_img);

    rgb32_pixel_t col;
    long long pix_count = 0;
    for (int oy = 0; oy < sview.height(); ++oy) {
        for (int ox = 0; ox < sview.width(); ++ox) {
            int bx = md.coords.x - src_img.width() / 2 + ox;
            int by = md.coords.y - src_img.height() / 2 + oy;

            // Check if within base image bounds
            if (bx < 0 || bx >= bview.width() || by < 0 || by >= bview.height()) continue;
            const auto bp = bview(bx, by); // Base pixel

            get_color(col, red_t()) += get_color(bp, red_t());
            get_color(col, green_t()) += get_color(bp, green_t());
            get_color(col, blue_t()) += get_color(bp, blue_t());
            pix_count++;
        }
    }
    if (pix_count) {
        get_color(col, red_t()) = get_color(col, red_t()) / pix_count;
        get_color(col, green_t()) = get_color(col, green_t()) / pix_count;
        get_color(col, blue_t()) = get_color(col, blue_t()) / pix_count;
    }
    return transform_image(templates[md.idx], md.deg, md.sz_mul, col);
}

shape_metadata Shaper::generateShapeData() {
    shape_metadata md;
    md.idx = lrand(0, templates.size());
    const auto &src_img = templates[md.idx];

    md.coords = {
        lrand(coords_bounds.xlow, coords_bounds.xhigh),
        lrand(coords_bounds.ylow, coords_bounds.yhigh),
    };

    // Resize in context of base image
    double max_size_mul = std::max(base_img.dimensions().x, base_img.dimensions().y) * 1.
                          / std::max(src_img.dimensions().x, src_img.dimensions().y);

    // Though it can be bigger
    max_size_mul += 1;

    md.sz_mul = drand(0, max_size_mul);
    md.deg = drand(0, 360);

    return md;
}
