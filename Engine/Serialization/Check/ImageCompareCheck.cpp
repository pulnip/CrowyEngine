#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <exception>
#include <format>
#include <print>
#include <span>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "ImageCompare.hpp"
#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "ImageWriter.hpp"
#include "Primitives.hpp"
#include "RHIUtil.hpp"
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
                "usage: ImageCompareCheck                   self-test\n"
                "       ImageCompareCheck <a> <b> [--tolerance N]\n"
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

        u8 parseTolerance(StrView text) {
            u32 value = 0;
            const auto end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if(ec != std::errc{} || ptr != end || value > 255) {
                throw std::runtime_error(
                    std::format(
                        "'--tolerance' takes a whole number from 0 to 255, not "
                        "'{}'\n{}",
                        text,
                        usage()
                    )
                );
            }

            return static_cast<u8>(value);
        }

        u32 parseRadius(StrView text) {
            u32 value = 0;
            const auto end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if(ec != std::errc{} || ptr != end || value > 64) {
                throw std::runtime_error(
                    std::format(
                        "'--radius' takes a whole number from 0 to 64, not "
                        "'{}'\n{}",
                        text,
                        usage()
                    )
                );
            }

            return value;
        }

        f64 parseNumber(StrView flag, StrView text, f64 low, f64 high) {
            f64 value = 0.0;
            const auto end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if(ec != std::errc{} || ptr != end || !std::isfinite(value) ||
               value < low || value > high) {
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

            return value;
        }

        f64 parseFraction(StrView flag, StrView text) {
            f64 value = 0.0;
            const auto end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);

            if(ec != std::errc{} || ptr != end || !std::isfinite(value) ||
               value < 0.0 || value > 1.0) {
                throw std::runtime_error(
                    std::format(
                        "'{}' takes a fraction from 0 to 1, not '{}'\n{}",
                        flag,
                        text,
                        usage()
                    )
                );
            }

            return value;
        }

        // the arguments after the program name
        Args parseArgs(std::span<char* const> args) {
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
                    parsed.edges.radius = parseRadius(value());
                    edgeFlags = true;
                    continue;
                }
                if(arg == "--density") {
                    const auto text = value();
                    parsed.edges.density = parseFraction(arg, text);
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
                    parsed.edges.lenience = parseNumber(arg, value(), 1.0, 8.0);
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
                    parsed.tolerance.channelDelta = parseTolerance(value());
                    compareFlags = true;
                    continue;
                }
                if(arg == "--max-fail") {
                    parsed.tolerance.maxGlobalFail =
                        parseFraction(arg, value());
                    compareFlags = true;
                    continue;
                }
                if(arg == "--max-local") {
                    parsed.tolerance.maxLocalFail = parseFraction(arg, value());
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
            } else if(convert) {
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
            } else if(parsed.paths.size() != 2) {
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

        // "similar", or "different:" and the thresholds that failed
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

        void writeImage(const Str& path, Rgba8View image) {
            if(path.ends_with(".png")) {
                writePng(toPath(path.c_str()), image);
                return;
            }

            const bool written = WriteBMP(
                image.pixels,
                image.rowPitch,
                image.width,
                image.height,
                false,
                path
            );
            if(!written) {
                throw std::runtime_error(
                    std::format("cannot write '{}'", path)
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
                    args.diff,
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

        // the report, then its numbers in one tab-separated row for scripts
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
            } else {
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
                    args.overlay,
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
            writeImage(args.paths[1], viewRgba8(image));

            return 0;
        }

        std::vector<u8> solid(u32 width, u32 height, u8 value) {
            const auto size = static_cast<usize>(width) * height * 4;
            std::vector<u8> pixels(size, value);
            for(usize i = 3; i < pixels.size(); i += 4)
                pixels[i] = 255;

            return pixels;
        }

        Rgba8View viewOf(const std::vector<u8>& pixels, u32 width, u32 height) {
            return Rgba8View{
                .pixels = pixels.data(),
                .width = width,
                .height = height,
                .rowPitch = static_cast<usize>(width) * 4
            };
        }

        bool checkIdentical() {
            std::println("Identical images");

            const auto a = solid(16, 16, 128);
            const auto b = solid(16, 16, 128);
            const auto comparison =
                compareImages(viewOf(a, 16, 16), viewOf(b, 16, 16));
            std::println("  16x16: {}", verdict(comparison));
            if(!comparison.similar || comparison.failing != 0) {
                std::println("  FAIL: an image differs from itself");
                return false;
            }

            return true;
        }

        bool checkOnePixel() {
            std::println("One pixel off by 3");

            const auto a = solid(16, 16, 128);
            auto b = a;
            b[(5 * 16 + 7) * 4] += 3;
            const auto viewA = viewOf(a, 16, 16);
            const auto viewB = viewOf(b, 16, 16);
            const auto strict =
                compareImages(viewA, viewB, {.channelDelta = 2});
            const auto loose = compareImages(viewA, viewB, {.channelDelta = 3});
            std::println("  tolerance 2: {}", verdict(strict));
            std::println("  tolerance 3: {}", verdict(loose));

            bool ok = true;
            if(strict.similar || strict.failing != 1) {
                std::println("  FAIL: tolerance 2 let the pixel through");
                ok = false;
            }
            if(!loose.similar || loose.failing != 0) {
                std::println("  FAIL: tolerance 3 failed the pixel");
                ok = false;
            }

            return ok;
        }

        bool checkSizeMismatch() {
            std::println("Size mismatch");

            const auto a = solid(4, 4, 0);
            const auto b = solid(4, 5, 0);
            try {
                compareImages(viewOf(a, 4, 4), viewOf(b, 4, 5));
            } catch(const std::invalid_argument& e) {
                std::println("  refused: {}", e.what());
                return true;
            }

            std::println("  FAIL: 4x4 was compared against 4x5");
            return false;
        }

        bool checkEdges() {
            std::println("A bar against itself moved (4, 0)");

            constexpr u32 Size = 64;
            auto a = solid(Size, Size, 0);
            auto b = a;
            for(u32 y = 16; y < 40; ++y) {
                for(u32 x = 20; x < 30; ++x) {
                    for(u32 c = 0; c < 3; ++c) {
                        a[(y * Size + x) * 4 + c] = 200;
                        b[(y * Size + x + 4) * 4 + c] = 200;
                    }
                }
            }
            const auto comparison =
                compareEdges(viewOf(a, Size, Size), viewOf(b, Size, Size));
            std::println(
                "  shift ({},{}), {} coincide",
                comparison.shiftX,
                comparison.shiftY,
                percent(comparison.shiftCoincide)
            );
            if(comparison.shiftX != 4 || comparison.shiftY != 0 ||
               comparison.shiftCoincide != 1.0) {
                std::println("  FAIL: the shift is not (4,0) at 100%");
                return false;
            }

            return true;
        }

        int selfTest() {
            bool ok = true;
            ok = checkIdentical() && ok;
            ok = checkOnePixel() && ok;
            ok = checkSizeMismatch() && ok;
            ok = checkEdges() && ok;
            if(!ok)
                return 1;

            std::println("Succeed!");
            return 0;
        }
    }
}

int main(int argc, char** argv) {
    using namespace Crowy;

    try {
        const auto args =
            std::span<char* const>(argv, static_cast<usize>(argc));
        if(args.size() <= 1)
            return selfTest();

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
    } catch(const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 2;
    }

    return 2;
}
