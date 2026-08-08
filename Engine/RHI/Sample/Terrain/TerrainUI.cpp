#include <algorithm>
#include <utility>
#include "TerrainUI.hpp"
#include "Noise.hpp"

namespace{
    using namespace Crowy;

    // every parameter control funnels through these, so none can forget
    // paramsDirty
    Widget ParamSlider(
        Str label,
        f32 TerrainParams::* field,
        const UIContext& ctx,
        f32 min, f32 max
    ){
        return Slider{
            .label = std::move(label),
            .onChanged = [field](UIContext& c, f32 v){
                c.params.*field = v;
                c.paramsDirty = true;
            },
            .v = ctx.params.*field,
            .v_min = min,
            .v_max = max
        };
    }

    Widget StatInt(Str label, const UIContext& ctx, u32 TerrainStats::* field){
        return IntField{
            .label = std::move(label),
            .get = [&ctx, field]{
                return static_cast<int>(ctx.stats.*field);
            }
        };
    }

    Widget StatFloat(Str label, const UIContext& ctx, f64 TerrainStats::* field){
        return FloatField{
            .label = std::move(label),
            .get = [&ctx, field]{
                return static_cast<f32>(ctx.stats.*field);
            }
        };
    }

    Widget BuildPanel(UIContext& ctx, const std::vector<Widget>& sampleStats){
        std::vector<Widget> children{
            Text{.data = "Terrain Params"},
            IntField{
                .label = "seed",
                .onChanged = [](UIContext& c, int v){
                    c.params.seed = static_cast<u32>(v);
                    c.paramsDirty = true;
                },
                .v = static_cast<int>(ctx.params.seed)
            },
            TextButton{
                .label = "Randomize",
                .onPressed = [](UIContext& c){
                    // xorshift32 on the live seed; 0 is a fixed point,
                    // hence the nudge
                    u32 s = c.params.seed != 0 ? c.params.seed : 1u;
                    s ^= s << 13;
                    s ^= s >> 17;
                    s ^= s << 5;
                    c.params.seed = s;

                    c.paramsDirty = true;
                    c.panelDirty = true;
                }
            },
            IntField{
                .label = "octaves",
                .onChanged = [](UIContext& c, int v){
                    const int clamped = std::clamp(static_cast<u32>(v), 1u, MAX_OCTAVES);
                    // the field shows the typed value until the tree is rebuilt
                    if(clamped != v)
                        c.panelDirty = true;

                    c.params.octaves = clamped;
                    c.paramsDirty = true;
                },
                .v = static_cast<i32>(ctx.params.octaves)
            },
            ParamSlider("freq", &TerrainParams::freq, ctx, 0.002f, 0.08f),
            // height caps keep the surface inside the grid
            ParamSlider("heightBase", &TerrainParams::heightBase, ctx, 4.0f, 48.0f),
            ParamSlider("heightAmp", &TerrainParams::heightAmp, ctx, 0.0f, 24.0f),

            Text{.data = "Cave Params"},
            ParamSlider("caveFreq", &TerrainParams::caveFreq, ctx, 0.01f, 0.2f),
            ParamSlider("caveThreshold", &TerrainParams::caveThreshold, ctx, 0.0f, 1.0f),
            // the useful range starts around 60 - see TerrainParams for why
            ParamSlider("caveStrength", &TerrainParams::caveStrength, ctx, 0.0f, 200.0f),

            Text{.data = "Rebuild"},
            TextButton{
                .label = "Rebuild",
                .onPressed = [](UIContext& c){
                    c.rebuildRequested = true;
                }
            },
            Checkbox{
                .label = "auto-rebuild",
                .onChanged = [](UIContext& c, bool v){
                    c.autoRebuild = v;
                },
                .v = ctx.autoRebuild
            },

            Text{.data = "Debug"},
            Checkbox{
                .label = "normals",
                .onChanged = [](UIContext& c, bool v){
                    c.debug.showNormals = v;
                },
                .v = ctx.debug.showNormals
            },
            Checkbox{
                .label = "wireframe",
                .onChanged = [](UIContext& c, bool v){
                    c.debug.wireframe = v;
                },
                .v = ctx.debug.wireframe
            },

            Text{.data = "Stats"},
            StatInt("vertices", ctx, &TerrainStats::vertexCount),
            StatInt("triangles", ctx, &TerrainStats::triangleCount),
            StatFloat("build (ms)", ctx, &TerrainStats::buildTimeMs),
            StatFloat("frame (ms)", ctx, &TerrainStats::frameTimeMs)
        };

        children.append_range(sampleStats);

        return Column(std::move(children));
    }
}

namespace Crowy
{
    TerrainPanel::TerrainPanel(UIContext& ctx, std::vector<Widget>&& sampleStats)
        : sampleStats(std::move(sampleStats))
        , widget(BuildPanel(ctx, this->sampleStats)){}

    Widget& TerrainPanel::Get(UIContext& ctx){
        if(ctx.panelDirty){
            widget = BuildPanel(ctx, sampleStats);
            ctx.panelDirty = false;
        }

        return widget;
    }
}
