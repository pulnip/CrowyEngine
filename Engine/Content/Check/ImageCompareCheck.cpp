#include <array>
#include <cstdio>
#include <exception>
#include <format>
#include <limits>
#include <print>
#include <span>
#include <stdexcept>
#include <vector>

#include "ImageCompare.hpp"
#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "ImageWriter.hpp"
#include "PathUtil.hpp"
#include "Primitives.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        enum class Mode : u8 { Compare, Convert, Edges, Help };

        struct Args {
            Mode mode = Mode::Compare;
            std::vector<Str> paths;
            ImageTolerance tolerance;
            Str diff;
            EdgeOptions edges;
            Str overlay;
        };

        Str usage() {
            constexpr ImageTolerance Defaults{};
            constexpr EdgeOptions EdgeDefaults{};

            return std::format(
                "usage: ImageCompareCheck <a> <b> [--tolerance N]\n"
                "                         [--max-fail F] [--max-local F]\n"
                "                         [--alpha] [--diff <out.png|.bmp>]\n"
                "       ImageCompareCheck --convert <in> <out.png|.bmp>\n"
                "       ImageCompareCheck --edges <a> <b> [--radius N]\n"
                "                         [--density F] [--lenience F]\n"
                "                         [--overlay <out.png|.bmp>]\n"
                "  --tolerance  largest difference a channel may have, 0-255 "
                "(default {})\n"
                "  --max-fail   fraction of all pixels that may fail "
                "(default {})\n"
                "  --max-local  fraction of the worst tile's pixels that may "
                "fail, the tiles a 10 x 10 grid (default {})\n"
                "  --alpha      compare alpha too\n"
                "  --edges      where the two pictures' thinned luma edges "
                "agree, a measure of placement that ignores tone\n"
                "  --radius     pixels within which an edge finds the "
                "other's, 0-64 (default {})\n"
                "  --density    fraction of the pixels each picture keeps as "
                "edges, strongest first (default {})\n"
                "  --lenience   how many times more edges the side searched "
                "keeps, 1-8 (default {})\n"
                "exit 0 when similar, converted or measured, 1 when "
                "different, 2 on an error",
                Defaults.channelDelta,
                Defaults.maxGlobalFail,
                Defaults.maxLocalFail,
                EdgeDefaults.radius,
                EdgeDefaults.density,
                EdgeDefaults.lenience
            );
        }

        bool isImageOutput(StrView path) {
            return path.ends_with(".png") || path.ends_with(".bmp");
        }

        template<Number T>
        T parseFlag(StrView flag, StrView text, T low, T high) {
            const auto value = parseNumber<T>(text);
            if(!value || !(low <= *value && *value <= high)) {
                throw std::runtime_error(
                    std::format(
                        "'{}' takes a number from {} to {}, not '{}'\n{}",
                        flag,
                        low,
                        high,
                        text,
                        usage()
                    )
                );
            }

            return *value;
        }

        Args parseArgs(std::span<char* const> args) {
            constexpr u32 MaxEdgeRadius = 64;

            Args parsed;
            bool convert = false;
            bool edges = false;
            bool compareFlags = false;
            bool edgeFlags = false;
            for(usize i = 0; i < args.size(); ++i) {
                const StrView arg = args[i];
                const auto value = [&] {
                    if(i + 1 == args.size()) {
                        throw std::runtime_error(
                            std::format("'{}' needs a value\n{}", arg, usage())
                        );
                    }
                    return StrView(args[++i]);
                };

                if(arg == "--help" || arg == "-h") {
                    parsed.mode = Mode::Help;
                    return parsed;
                }
                if(arg == "--convert") {
                    convert = true;
                    continue;
                }
                if(arg == "--edges") {
                    edges = true;
                    continue;
                }
                if(arg == "--radius") {
                    parsed.edges.radius =
                        parseFlag<u32>(arg, value(), 0, MaxEdgeRadius);
                    edgeFlags = true;
                    continue;
                }
                if(arg == "--density") {
                    const auto text = value();
                    parsed.edges.density = parseFlag(arg, text, 0.0, 1.0);
                    if(parsed.edges.density == 0.0) {
                        throw std::runtime_error(
                            std::format(
                                "'--density' keeps some edges, not '{}'\n{}",
                                text,
                                usage()
                            )
                        );
                    }
                    edgeFlags = true;
                    continue;
                }
                if(arg == "--lenience") {
                    parsed.edges.lenience = parseFlag(arg, value(), 1.0, 8.0);
                    edgeFlags = true;
                    continue;
                }
                if(arg == "--overlay") {
                    parsed.overlay = value();
                    edgeFlags = true;
                    continue;
                }
                if(arg == "--alpha") {
                    parsed.tolerance.compareAlpha = true;
                    compareFlags = true;
                    continue;
                }
                if(arg == "--tolerance") {
                    parsed.tolerance.channelDelta = parseFlag<u8>(
                        arg,
                        value(),
                        0,
                        std::numeric_limits<u8>::max()
                    );
                    compareFlags = true;
                    continue;
                }
                if(arg == "--max-fail") {
                    parsed.tolerance.maxGlobalFail =
                        parseFlag(arg, value(), 0.0, 1.0);
                    compareFlags = true;
                    continue;
                }
                if(arg == "--max-local") {
                    parsed.tolerance.maxLocalFail =
                        parseFlag(arg, value(), 0.0, 1.0);
                    compareFlags = true;
                    continue;
                }
                if(arg == "--diff") {
                    parsed.diff = value();
                    compareFlags = true;
                    continue;
                }
                if(arg.starts_with("-")) {
                    throw std::runtime_error(
                        std::format("unknown argument '{}'\n{}", arg, usage())
                    );
                }
                parsed.paths.emplace_back(arg);
            }

            if(edgeFlags && !edges) {
                throw std::runtime_error(
                    std::format(
                        "'--radius', '--density', '--lenience' and "
                        "'--overlay' go with '--edges'\n{}",
                        usage()
                    )
                );
            }
            if(edges) {
                if(convert || compareFlags || parsed.paths.size() != 2) {
                    throw std::runtime_error(
                        std::format(
                            "'--edges' takes two images and only its own "
                            "flags\n{}",
                            usage()
                        )
                    );
                }
                parsed.mode = Mode::Edges;
            }
            else if(convert) {
                if(compareFlags || parsed.paths.size() != 2) {
                    throw std::runtime_error(
                        std::format(
                            "'--convert' takes an input and an output, and no "
                            "other flag\n{}",
                            usage()
                        )
                    );
                }
                parsed.mode = Mode::Convert;
            }
            else if(parsed.paths.size() != 2) {
                throw std::runtime_error(
                    std::format(
                        "expected two images to compare, got {}\n{}",
                        parsed.paths.size(),
                        usage()
                    )
                );
            }

            const auto output = [&]() -> const Str& {
                switch(parsed.mode) {
                case Mode::Convert:
                    return parsed.paths[1];
                case Mode::Edges:
                    return parsed.overlay;
                default:
                    return parsed.diff;
                }
            }();
            if(!output.empty() && !isImageOutput(output)) {
                throw std::runtime_error(
                    std::format(
                        "'{}' must end in .png or .bmp\n{}",
                        output,
                        usage()
                    )
                );
            }

            return parsed;
        }

        Str percent(f64 fraction) {
            return std::format("{:.4f}%", fraction * 100.0);
        }

        Str verdict(const ImageComparison& comparison) {
            if(comparison.similar)
                return "similar";

            const auto& tolerance = comparison.tolerance;
            Str reasons;
            if(comparison.globalFail > tolerance.maxGlobalFail) {
                reasons += std::format(
                    " global {} > {}",
                    percent(comparison.globalFail),
                    percent(tolerance.maxGlobalFail)
                );
            }
            if(comparison.localFail > tolerance.maxLocalFail) {
                reasons += std::format(
                    "{} local {} > {}",
                    reasons.empty() ? "" : ",",
                    percent(comparison.localFail),
                    percent(tolerance.maxLocalFail)
                );
            }

            return "different:" + reasons;
        }

        void report(StrView label, const ImageComparison& comparison) {
            const auto& tolerance = comparison.tolerance;
            const auto& tile = comparison.worstTile;
            std::println(
                "{}: {}x{}, tolerance {}, alpha {}",
                label,
                comparison.width,
                comparison.height,
                tolerance.channelDelta,
                tolerance.compareAlpha ? "compared" : "ignored"
            );
            std::println(
                "  failing {} of {} ({}), limit {}",
                comparison.failing,
                static_cast<u64>(comparison.width) * comparison.height,
                percent(comparison.globalFail),
                percent(tolerance.maxGlobalFail)
            );
            std::println(
                "  worst tile ({},{}) x {}..{} y {}..{}: {} failing, limit {}",
                comparison.tileX,
                comparison.tileY,
                tile.x0,
                tile.x1,
                tile.y0,
                tile.y1,
                percent(comparison.localFail),
                percent(tolerance.maxLocalFail)
            );

            constexpr std::array<char, 4> Names = {'R', 'G', 'B', 'A'};
            const usize channels = tolerance.compareAlpha ? 4 : 3;
            for(usize c = 0; c < channels; ++c) {
                const auto& stats = comparison.channel[c];
                std::println(
                    "  {} max {} mean {:.3f} rms {:.3f} psnr {:.2f} dB",
                    Names[c],
                    stats.max,
                    stats.mean,
                    stats.rms,
                    stats.psnr
                );
            }
        }

        int compareFiles(const Args& args) {
            const auto& pathA = args.paths[0];
            const auto& pathB = args.paths[1];
            const auto a = LoadImage(toPath(pathA.c_str()));
            const auto b = LoadImage(toPath(pathB.c_str()));
            const auto comparison =
                compareImages(viewRgba8(a), viewRgba8(b), args.tolerance);

            report(std::format("{} vs {}", pathA, pathB), comparison);
            if(!args.diff.empty()) {
                const auto heat = paintHeatMap(comparison);
                writeImage(
                    toPath(args.diff),
                    Rgba8View{
                        .pixels = heat.data(),
                        .width = comparison.width,
                        .height = comparison.height,
                        .rowPitch = static_cast<usize>(comparison.width) * 4
                    }
                );
                std::println("  heat map: {}", args.diff);
            }
            std::println("{}", verdict(comparison));

            return comparison.similar ? 0 : 1;
        }

        // Tools/stage_cuts.ps1 and .sh read the row's columns by position
        int compareEdgeFiles(const Args& args) {
            const auto& pathA = args.paths[0];
            const auto& pathB = args.paths[1];
            const auto a = LoadImage(toPath(pathA.c_str()));
            const auto b = LoadImage(toPath(pathB.c_str()));
            const auto comparison =
                compareEdges(viewRgba8(a), viewRgba8(b), args.edges);
            const auto& options = comparison.options;
            const auto& tile = comparison.worstTile;

            std::println(
                "edges {} vs {}: {}x{}, radius {}, density {}, lenience {}",
                pathA,
                pathB,
                comparison.width,
                comparison.height,
                options.radius,
                percent(options.density),
                options.lenience
            );
            std::println(
                "  a: {} edges, {} near b",
                comparison.edgesA,
                percent(comparison.aNearB)
            );
            std::println(
                "  b: {} edges, {} near a",
                comparison.edgesB,
                percent(comparison.bNearA)
            );
            if(comparison.worstTileEdges > 0) {
                std::println(
                    "  worst tile ({},{}) x {}..{} y {}..{}: {} of {} edges "
                    "near b",
                    comparison.tileX,
                    comparison.tileY,
                    tile.x0,
                    tile.x1,
                    tile.y0,
                    tile.y1,
                    percent(comparison.worstTileNear),
                    comparison.worstTileEdges
                );
            }
            else {
                std::println(
                    "  worst tile: none holds {} of a's edges",
                    options.minTileEdges
                );
            }
            std::println(
                "  shift ({},{}): {} coincide, at (0,0) {}",
                comparison.shiftX,
                comparison.shiftY,
                percent(comparison.shiftCoincide),
                percent(comparison.zeroCoincide)
            );
            if(!args.overlay.empty()) {
                const auto overlay = paintEdgeOverlay(comparison, viewRgba8(a));
                writeImage(
                    toPath(args.overlay),
                    Rgba8View{
                        .pixels = overlay.data(),
                        .width = comparison.width,
                        .height = comparison.height,
                        .rowPitch = static_cast<usize>(comparison.width) * 4
                    }
                );
                std::println("  overlay: {}", args.overlay);
            }

            const bool ranked = comparison.worstTileEdges > 0;
            const auto tileAt =
                std::format("{},{}", comparison.tileX, comparison.tileY);
            const auto tileNear =
                std::format("{:.4f}", comparison.worstTileNear);
            std::println(
                "row\t{}\t{}\t{:.4f}\t{:.4f}\t{}\t{}\t{}\t{},{}\t{:.4f}"
                "\t{:.4f}",
                comparison.edgesA,
                comparison.edgesB,
                comparison.aNearB,
                comparison.bNearA,
                ranked ? tileAt : Str("-"),
                ranked ? tileNear : Str("-"),
                comparison.worstTileEdges,
                comparison.shiftX,
                comparison.shiftY,
                comparison.shiftCoincide,
                comparison.zeroCoincide
            );

            return 0;
        }

        int convertFile(const Args& args) {
            const auto image = LoadImage(toPath(args.paths[0].c_str()));
            writeImage(toPath(args.paths[1]), viewRgba8(image));

            return 0;
        }
    }
}

int main(int argc, char** argv) {
    using namespace Crowy;

    try {
        const auto args =
            std::span<char* const>(argv, static_cast<usize>(argc));
        const auto parsed = parseArgs(args.subspan(1));
        switch(parsed.mode) {
        case Mode::Help:
            std::println("{}", usage());
            return 0;
        case Mode::Convert:
            return convertFile(parsed);
        case Mode::Edges:
            return compareEdgeFiles(parsed);
        case Mode::Compare:
            return compareFiles(parsed);
        }
    }
    catch(const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 2;
    }

    return 2;
}
