#include "ImageOps.h"

#include <boost/gil/image.hpp>
#include <boost/gil/io/read_image.hpp>

#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>

using namespace boost::gil;


alpha_img_t grayscale_filter(const alpha_img_t &img) {
    alpha_img_t out_img(img.dimensions());

    auto out = view(out_img);
    auto in = const_view(img);
    for (int y = 0; y < in.height(); ++y) {
        for (int x = 0; x < in.width(); ++x) {
            auto src = in(x, y);

            gray8_pixel_t gray;
            color_convert(src, gray);

            color_convert(gray, out(x, y));

            get_color(out(x, y), alpha_t()) = get_color(src, alpha_t());
        }
    }
    return out_img;
}


alpha_img_t grayscale_colorize(const alpha_img_t &img, const pix_t &col) {
    alpha_img_t out_img(img.dimensions());

    auto out = view(out_img);
    auto in = const_view(img);
    for (int y = 0; y < in.height(); ++y) {
        for (int x = 0; x < in.width(); ++x) {
            auto src = in(x, y);

            gray8_pixel_t gray;
            color_convert(src, gray);

            double g = get_color(gray, gray_color_t()) / 255.;
            alpha_pix_t pix(
                std::min(static_cast<int>(get_color(col, red_t()) * g), 255),
                std::min(static_cast<int>(get_color(col, green_t()) * g), 255),
                std::min(static_cast<int>(get_color(col, blue_t()) * g), 255),
                get_color(src, alpha_t())
            );

            out(x, y) = pix;
        }
    }
    return out_img;
}

alpha_img_t scale_image(const alpha_img_t &img, const point<int> &sz) {
    alpha_img_t resized_img(sz.x, sz.y);
    resize_view(const_view(img), view(resized_img), nearest_neighbor_sampler());
    return resized_img;
}


alpha_img_t transform_image(const alpha_img_t &img, double deg, double sz_mul, const pix_t &col) {
    auto new_dim = img.dimensions() * sz_mul;
    auto resized_img =
            scale_image(img, {static_cast<int>(new_dim.x), static_cast<int>(new_dim.y)});

    alpha_img_t rotated_img(resized_img.dimensions());
    matrix3x2<double> mat =
            matrix3x2<double>::get_translate(-resized_img.dimensions() / 2.) *
            // matrix3x2<double>::get_scale(point(1 / size_mul, 1 / size_mul)) *
            matrix3x2<double>::get_rotate(deg * M_PI / 180.) *
            matrix3x2<double>::get_translate(resized_img.dimensions() / 2.);

    resample_pixels(const_view(resized_img), view(rotated_img), mat, nearest_neighbor_sampler());

    auto col_img = grayscale_colorize(rotated_img, col);

    return col_img;
}

alpha_img_t read_png_or_jpg(const std::string &path) {
    alpha_img_t img;

    try {
        read_image(path, img, png_tag());
    } catch (std::ios_base::failure &) {
        img_t jpg;
        read_image(path, jpg, jpeg_tag());

        img.recreate(jpg.width(), jpg.height());
        copy_and_convert_pixels(
            const_view(jpg),
            view(img)
        );
    }
    return img;
}

int color_similarity_score(const alpha_pix_t &c1, const alpha_pix_t &c2) {
    double sim = 1 - (std::abs(get_color(c1, red_t()) - get_color(c2, red_t()))
                      + std::abs(get_color(c1, green_t()) - get_color(c2, green_t()))
                      + std::abs(get_color(c1, blue_t()) - get_color(c2, blue_t()))) / 255. * 3;
    return static_cast<int>(sim * 100);
}

int overlay_compare(const alpha_img_t &base_img, alpha_img_t &canvas, const alpha_img_t &shape,
                    point<int> coords, bool set) {
    if (base_img.dimensions() != canvas.dimensions()) {
        std::cerr << "Base image and canvas are not the same size\n";
        return -1;
    }

    int sd = 0;
    auto sview = const_view(shape);
    auto cview = view(canvas);
    auto bview = const_view(base_img);

    for (int oy = 0; oy < sview.height(); ++oy) {
        for (int ox = 0; ox < sview.width(); ++ox) {
            int bx = coords.x - shape.width() / 2 + ox;
            int by = coords.y - shape.height() / 2 + oy;

            // Check if within base image bounds
            if (bx < 0 || bx >= cview.width() || by < 0 || by >= cview.height()) continue;

            auto shp = sview(ox, oy); // Overlay pixel
            const auto cp = cview(bx, by); // Base pixel

            // Normalize alpha values to [0, 1]
            float salpha = get_color(shp, alpha_t()) / 255.0f;

            // Extract RGB components
            float src_r = at_c<0>(shp);
            float src_g = at_c<1>(shp);
            float src_b = at_c<2>(shp);

            float dst_r = at_c<0>(cp);
            float dst_g = at_c<1>(cp);
            float dst_b = at_c<2>(cp);

            // Blend RGB channels using alpha compositing formula
            float out_r = (src_r * salpha + dst_r * (1 - salpha));
            float out_g = (src_g * salpha + dst_g * (1 - salpha));
            float out_b = (src_b * salpha + dst_b * (1 - salpha));

            // Convert back to 8-bit values
            alpha_pix_t newp(
                static_cast<uint8_t>(std::round(out_r)),
                static_cast<uint8_t>(std::round(out_g)),
                static_cast<uint8_t>(std::round(out_b)),
                255);

            // Update the delta
            sd -= color_similarity_score(bview(bx, by), cview(bx, by));
            sd += color_similarity_score(bview(bx, by), newp);
            if (set) {
                cview(bx, by) = newp;
            }
        }
    }
    return sd;
}
