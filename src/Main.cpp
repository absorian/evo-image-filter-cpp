#include <iostream>
#include <filesystem>

#include <boost/gil/image.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/io/write_view.hpp>

#include <args.hxx>

#include "ImageOps.h"
#include "Parallelizer.h"
#include "Shaper.h"
#include "StepSorter.h"
#include "Timestamper.h"


struct shape_candidate {
    int64_t score_delta;
    shape_metadata md;
};

enum image_extension {
    EXT_PNG,
    EXT_JPG
};

#define map_range(a1,a2,b1,b2,s) (b1 + (s - a1) * (b2 - b1) / (a2 - a1))
#define pretty_score(overall, score) map_range(0, (overall * 100), 0, 10000, score)

int main(int argc, char **argv) {
    args::ArgumentParser parser(
        "Image filter that recreates the source from"
        " combination of shapes using evolution/mutation.",
        "absorian");

    args::HelpFlag arg_help(parser, "help", "Display this help menu", {'h', "help"});

    args::Positional<std::string> arg_image(parser, "image",
                                            "Source image, must be of jpg/png type",
                                            {args::Options::Required});
    args::Positional<std::string> arg_shapes_dir(parser, "shapes_dir",
                                                 "Directory with shapes, must be of jpg/png type",
                                                 {args::Options::Required});
    args::ValueFlag<std::string> arg_output(parser, "output",
                                            "Output path, extension must be of png type",
                                            {'o', "output"});

    args::ValueFlag<int> arg_shapes_count(parser, "shapes_count",
                                          "Number of shapes to put into the final image",
                                          {'s', "shapes"}, 2000);

    args::ValueFlag<int> arg_score_threshold(parser, "score_threshold",
                                              "Stop score in range 0..10000",
                                              {'t', "threshold"}, -1);

    args::ValueFlag<int> arg_initial_swarm(parser, "initial_swarm",
                                           "Number of shapes in initial swarm",
                                           {"swarm"}, 800);
    args::ValueFlag<int> arg_survived_count(parser, "survived_count",
                                            "Number of survived shapes after one cycle",
                                            {"survived"}, 150);
    args::ValueFlag<int> arg_children_count(parser, "children_count",
                                            "Number of children for each survived shape",
                                            {"children"}, 5);
    args::ValueFlag<int> arg_generations_count(parser, "generations_count",
                                               "Number of generations to simulate before choosing the shape",
                                               {"generations"}, 5);

    args::ValueFlag<int> arg_threads_count(parser, "threads_count",
                                           "Number of threads to utilize",
                                           {'j'}, 16);
    args::ValueFlag<int> arg_shapes_per_save(parser, "shapes_per_save",
                                             "Each N of added shapes the program will save the canvas",
                                             {"shapes-per-save"}, 5);

    args::ValueFlagList<int> arg_shape_resize(parser, "shape_resize",
                                              "Resize shapes to specified resolution, if one value is passed,"
                                              "shape will be N*N, if two values - N*M",
                                              {"shape-resize"});
    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help &) {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (const args::ValidationError &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    const int children_count = args::get(arg_children_count);
    const int initial_shapes_create_count = args::get(arg_initial_swarm);
    const int top_shapes_count = args::get(arg_survived_count);
    const int generations_count = args::get(arg_generations_count);

    const int score_threshold = args::get(arg_score_threshold);
    const int canvas_shapes_count = score_threshold > 0 ? INT_MAX : args::get(arg_shapes_count);
    const int threads_count = args::get(arg_threads_count);
    const int shapes_per_save = args::get(arg_shapes_per_save);

    std::filesystem::path img_path(args::get(arg_image));
    std::filesystem::path dir_path(args::get(arg_shapes_dir));

    if (status(img_path).type() != std::filesystem::file_type::regular
        || status(dir_path).type() != std::filesystem::file_type::directory) {
        throw std::invalid_argument("image and/or shapes_dir are invalid");
    }
    std::filesystem::path out_path;
    if (arg_output) {
        out_path.assign(args::get(arg_output));
        auto ext = out_path.extension();
        if (ext != ".png") {
            throw std::invalid_argument("output extension must be 'png'");
        }
    } else {
        out_path.assign("./" + img_path.stem().string() + "-out.png");
    }

    boost::gil::point<int> shape_sz{-1, -1};
    do {
        const auto &sz_arg = args::get(arg_shape_resize);
        if (sz_arg.empty()) break;
        if (sz_arg.size() > 2)
            throw std::invalid_argument("shape_resize accepts either one or two values");

        if (sz_arg[0] <= 0) {
            throw std::invalid_argument("shape_resize accepts only positive values");
        }
        shape_sz = {sz_arg[0], sz_arg[0]};
        if (sz_arg.size() < 2) break;

        if (sz_arg[1] <= 0) {
            throw std::invalid_argument("shape_resize accepts only positive values");
        }
        shape_sz.y = sz_arg[1];
    } while(false);

    Timestamper ts("prog");
    StepSorter ssc(top_shapes_count);
    Parallelizer pll(threads_count);

    Shaper shp(dir_path, shape_sz);
    std::cout << ts.stamp() << "Consumed shapes directory" << '\n';

    auto base_img = read_png_or_jpg(img_path);
    shp.setBaseImage(base_img);
    const long base_pix_count = base_img.height() * base_img.width();
    std::cout << ts.stamp() << "Retrieved base image" << '\n';

    std::vector<shape_candidate> shapes(ssc.storage_size);
    std::vector<std::pair<shape_candidate, int> > winners(top_shapes_count);
    alpha_img_t canvas(base_img.dimensions());
    std::vector<shape_candidate> best_from_gen(generations_count);

    fill_pixels(view(canvas), alpha_pix_t(0, 0, 0, 0));
    std::cout << ts.stamp() << "Created canvas" << '\n';

    long long score = 0;

    for (int csi = 0; csi < canvas_shapes_count; ++csi) {
        std::cout << "----------------------------------" << '\n';
        ts.sub("shape#" + std::to_string(csi + 1));
        ssc.call(shapes, initial_shapes_create_count,
                 [&](auto b_begin, auto b_end, int b_sz) {
                     pll.call<decltype(shapes)>(b_begin, b_sz, [&](auto bb_begin, auto bb_end) {
                         for (auto bb_it = bb_begin; bb_it != bb_end; ++bb_it) {
                             shape_candidate &sh = *bb_it;
                             sh.md = shp.generateShapeData();

                             sh.score_delta = overlay_compare(base_img, canvas,
                                                              shp.applyShapeData(sh.md), sh.md.coords);
                         }
                     });
                 },
                 [](const shape_candidate &a, const shape_candidate &b) {
                     return a.score_delta > b.score_delta;
                 });
        std::cout << ts.stamp() << "Initial swarm ready" << '\n';

        // move winners to another storage
        for (int wi = 0; wi < top_shapes_count; ++wi) {
            winners[wi].first = shapes[wi];
        }

        ts.sub("gen_mut");
        for (int gi = 0; gi < generations_count; ++gi) {
            for (int wi = 0; wi < top_shapes_count; ++wi) {
                winners[wi].second = children_count;
            }
            ssc.call(shapes, top_shapes_count * children_count,
                     [&](auto b_begin, auto b_end, int sz) {
                         std::atomic_int32_t w_i = 0;
                         pll.call<decltype(shapes)>(b_begin, sz,
                                                    [&](auto bb_begin, auto bb_end) {
                                                        while (bb_begin != bb_end) {
                                                            int t_w = w_i++;
                                                            if (t_w >= winners.size()) return;
                                                            auto &[w, ch_left] = winners[t_w];
                                                            while (ch_left > 0) {
                                                                if (bb_begin == bb_end) {
                                                                    return;
                                                                }

                                                                shape_candidate &sh = *bb_begin;
                                                                sh.md = shp.mutateShapeData(w.md);
                                                                sh.score_delta = overlay_compare(base_img, canvas,
                                                                    shp.applyShapeData(sh.md), sh.md.coords);
                                                                ++bb_begin;
                                                                --ch_left;
                                                            }
                                                        }
                                                    });
                     },
                     [](const shape_candidate &a, const shape_candidate &b) {
                         return a.score_delta > b.score_delta;
                     });
            std::cout << ts.stamp() << "#" << gi + 1
                    << ": best_raw_score_delta=" << shapes[0].score_delta << '\n';
            best_from_gen[gi] = shapes[0];
        }
        std::ranges::sort(best_from_gen,
                          [](const shape_candidate &a, const shape_candidate &b) {
                              return a.score_delta > b.score_delta;
                          });
        ts.out();

        const auto &winwin = best_from_gen[0];
        score += winwin.score_delta;
        overlay_compare(base_img, canvas, shp.applyShapeData(winwin.md), winwin.md.coords, true);

        int pr_sc = pretty_score(base_pix_count, score);
        std::cout << ts.stamp() << "Added shape, new_pretty_score=" << pr_sc << '\n';
        if ((csi + 1) % shapes_per_save == 0) {
            write_view(out_path, const_view(canvas), boost::gil::png_tag());
            std::cout << ts.stamp() << "Saved canvas" << '\n';
        }
        ts.out();
        if (score_threshold > 0
            && pr_sc >= score_threshold) {
            break;
        }
        ts.dry_out();
    }
    write_view(out_path, const_view(canvas), boost::gil::png_tag());

    return 0;
}
