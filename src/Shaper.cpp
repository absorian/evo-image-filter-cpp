

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

shape_metadata Shaper::mutateShapeData(const shape_metadata &md) {
    int r = get_color(md.col, red_t());
    int g = get_color(md.col, green_t());
    int b = get_color(md.col, blue_t());
    r = std::clamp(r + lrand(-50, 50), 0, 255);
    g = std::clamp(g + lrand(-50, 50), 0, 255);
    b = std::clamp(b + lrand(-50, 50), 0, 255);

    float base_img_mul = mut_boundaries_base_img_mul;
    point coords{
        lrand(-base_img_mul * md.base_img_dim.x,
              base_img_mul * md.base_img_dim.x),
        lrand(-base_img_mul * md.base_img_dim.y,
              base_img_mul * md.base_img_dim.y),
    };
    coords += md.coords;

    int xlow = -(gen_boundaries_size_mul - 1) * md.base_img_dim.x;
    int xhigh = md.base_img_dim.x * gen_boundaries_size_mul;
    int ylow = -(gen_boundaries_size_mul - 1) * md.base_img_dim.y;
    int yhigh = md.base_img_dim.y * gen_boundaries_size_mul;

    if (coords.x < xlow)
        coords.x = xlow;
    else if (coords.x > xhigh)
        coords.x = xhigh;

    if (coords.y < ylow)
        coords.y = ylow;
    else if (coords.y > yhigh)
        coords.y = yhigh;

    return {
        md.base_img_dim,
        coords,
        md.deg + drand(-60, 60),
        md.sz_mul * drand(0.5, 1.5),
        pix_t(r, g, b), md.idx
    };
}

alpha_img_t Shaper::applyShapeData(const shape_metadata &md) {
    return transform_image(templates[md.idx], md.deg, md.sz_mul, md.col);
}

shape_metadata Shaper::generateShapeData(const alpha_img_t &base_img) {
    shape_metadata md;
    md.idx = lrand(0, templates.size());
    const auto &src_img = templates[md.idx];

    md.base_img_dim = base_img.dimensions();
    int xlow = -(gen_boundaries_size_mul - 1) * md.base_img_dim.x;
    int xhigh = md.base_img_dim.x * gen_boundaries_size_mul;
    int ylow = -(gen_boundaries_size_mul - 1) * md.base_img_dim.y;
    int yhigh = md.base_img_dim.y * gen_boundaries_size_mul;
    md.coords = {
        lrand(xlow, xhigh),
        lrand(ylow, yhigh),
    };

    // Resize in context of base image
    double max_size_mul = std::max(base_img.dimensions().x, base_img.dimensions().y) * 1.
                          / std::max(src_img.dimensions().x, src_img.dimensions().y);

    // Though it can be bigger
    max_size_mul += 1;

    md.sz_mul = drand(0, max_size_mul);
    md.deg = drand(0, 360);

    get_color(md.col, red_t()) = lrand(0, 256);
    get_color(md.col, green_t()) = lrand(0, 256);
    get_color(md.col, blue_t()) = lrand(0, 256);

    return md;

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
        get_color(md.col, red_t()) = get_color(col, red_t()) / pix_count;
        get_color(md.col, green_t()) = get_color(col, green_t()) / pix_count;
        get_color(md.col, blue_t()) = get_color(col, blue_t()) / pix_count;
    }

    return md;
}
