#pragma once

#include "Types.h"

alpha_img_t colorize_mask(const alpha_img_t &img, const pix_t &col);

alpha_img_t scale_image(const alpha_img_t &img, const boost::gil::point<int>& sz);

alpha_img_t transform_image(const alpha_img_t &img, double deg, double sz_mul, const pix_t &col);

alpha_img_t read_png_or_jpg(const std::string &path);

int color_similarity_score(const alpha_pix_t &c1, const alpha_pix_t &c2);

int64_t overlay_compare(const alpha_img_t &base_img, alpha_img_t &canvas, const alpha_img_t &shape,
                    boost::gil::point<int> coords, bool set = false);

