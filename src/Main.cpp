#include <iostream>
#include <filesystem>
#include <mutex>

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
    int score_delta;
    shape_metadata md;

    std::future<void> fut;
};

enum image_extension {
    EXT_PNG,
    EXT_JPG
};

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

    args::ValueFlag<int> arg_initial_swarm(parser, "initial_swarm",
                                           "Number of shapes in initial swarm",
                                           {"init-swarm"}, 800);
    args::ValueFlag<int> arg_survived_count(parser, "survived_count",
                                            "Number of survived shapes after one cycle",
                                            {"survived-count"}, 150);
    args::ValueFlag<int> arg_children_count(parser, "children_count",
                                            "Number of children for each survived shape",
                                            {"children"}, 5);
    args::ValueFlag<int> arg_generations_count(parser, "generations_count",
                                               "Number of generations to simulate before choosing the shape",
                                               {"generations"}, 5);

    args::ValueFlag<int> arg_sortout_percent(parser, "sortout_percent",
                                             "Percentage of shapes storage above survived count, "
                                             "higher the faster, but with higher memory consumption",
                                             {"sortout-percent"}, 100);
    args::ValueFlag<int> arg_threads_count(parser, "threads_count",
                                       "Number of threads to utilize",
                                       {'j'}, 16);
    args::ValueFlag<int> arg_shapes_per_save(parser, "shapes_per_save",
                                       "Each N of added shapes the program will save the canvas",
                                       {"shapes-save"}, 5);
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

    const int canvas_shapes_count = args::get(arg_shapes_count);
    const int threads_count = args::get(arg_threads_count);
    const int shapes_per_save = args::get(arg_shapes_per_save);

    std::filesystem::path img_path(args::get(arg_image));
    std::filesystem::path dir_path(args::get(arg_shapes_dir));

    if (status(img_path).type() != std::filesystem::file_type::regular
        || status(dir_path).type() != std::filesystem::file_type::directory) {
        throw std::invalid_argument("Either source image path or shapes directory are invalid");
    }
    std::filesystem::path out_path;
    if (arg_output) {
        out_path.assign(args::get(arg_output));
        auto ext = out_path.extension();
        if (ext != ".png") {
            throw std::invalid_argument("Output image extension must be 'png'");
        }
    } else {
        out_path.assign("./" + img_path.stem().string() + "-out.png");
    }

    Timestamper ts("main");
    StepSorter ssc(top_shapes_count, args::get(arg_sortout_percent));
    Parallelizer pll(threads_count);

    Shaper shp(dir_path);
    std::cout << ts.stamp() << "Consumed shapes directory" << '\n';

    auto base_img = read_png_or_jpg(img_path);
    std::cout << ts.stamp() << "Retrieved base image" << '\n';

    std::vector<shape_candidate> shapes(ssc.storage_size);
    std::vector<shape_candidate> winners(top_shapes_count);
    alpha_img_t canvas(base_img.dimensions());

    fill_pixels(view(canvas), alpha_pix_t(128, 128, 128, 255));
    std::cout << ts.stamp() << "Created canvas" << '\n';

    int score = overlay_compare(base_img, base_img, canvas, {0, 0});
    std::cout << ts.stamp() << "Computed initial score: " << score << '\n';

    ts.sub("shapes");
    for (int csi = 0; csi < canvas_shapes_count; ++csi) {
        std::cout << "----------------------------------" << '\n';
        ts.sub("shape#" + std::to_string(csi + 1));
        ssc.call(shapes, initial_shapes_create_count,
                 [&](auto b_begin, auto b_end, int b_sz) {
                     pll.call<decltype(shapes)>(b_begin, b_sz, [&](auto bb_begin, auto bb_end) {
                         for (auto bb_it = bb_begin; bb_it != bb_end; ++bb_it) {
                             shape_candidate &sh = *bb_it;
                             sh.md = shp.generateShapeData(base_img);

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
            winners[wi] = std::move(shapes[wi]);
        }

        ts.sub("gen_mut");
        for (int gi = 0; gi < generations_count; ++gi) {
            ssc.call(shapes, top_shapes_count * children_count,
                     [&](auto b_begin, auto b_end, int sz) {
                         std::mutex mt;
                         pll.call(winners, top_shapes_count,
                                  [&](auto w_begin, auto w_end) {
                                      for (auto win_it = w_begin; win_it != w_end; ++win_it) {
                                          for (int ci = 0; ci < children_count; ++ci) {
                                              mt.lock();
                                              // TODO: research this if
                                              if (b_begin == b_end) {
                                                  mt.unlock();
                                                  return;
                                              }
                                              shape_candidate &sh = *b_begin;
                                              ++b_begin;
                                              mt.unlock();
                                              shape_candidate &w = *win_it;
                                              sh.md = shp.mutateShapeData(w.md);
                                              sh.score_delta = overlay_compare(base_img, canvas,
                                                                               shp.applyShapeData(sh.md), sh.md.coords);
                                          }
                                      }
                                  });
                     },
                     [](const shape_candidate &a, const shape_candidate &b) {
                         return a.score_delta > b.score_delta;
                     });
            std::cout << ts.stamp() << "#" << gi + 1 << ": best_score_delta=" << shapes[0].score_delta << '\n';
        }
        ts.out();

        const auto &winwin = shapes[0];
        score += winwin.score_delta;
        overlay_compare(base_img, canvas, shp.applyShapeData(winwin.md), winwin.md.coords, true);

        std::cout << ts.stamp() << "Added shape, new_score=" << score << '\n';
        if ((csi + 1) % shapes_per_save == 0) {
            write_view(out_path, const_view(canvas), boost::gil::png_tag());
            std::cout << ts.stamp() << "Saved canvas" << '\n';
        }
        ts.out();
    }
    ts.out();
    write_view(out_path, const_view(canvas), boost::gil::png_tag());

    return 0;
}
