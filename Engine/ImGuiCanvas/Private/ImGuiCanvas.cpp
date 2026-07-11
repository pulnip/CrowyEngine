#include <array>
#include <vector>
#include <imgui.h>
#include "Assert.hpp"
#include "Canvas2D.hpp"
#include "ImGuiCanvas.hpp"

namespace{
    ImVec2 toImVec2(Crowy::Vec2 v) noexcept{
        return ImVec2{v.x, v.y};
    }
    ImVec4 toImVec4(Crowy::Color c){
        return ImVec4{c.x, c.y, c.z, c.w};
    }

    ImU32 toImU32(Crowy::Color c) noexcept{
        return ImGui::ColorConvertFloat4ToU32(toImVec4(c));
    }
}

namespace Crowy
{
    // Column-Major
    using Mat2x3 = std::array<Vec2, 3>;

    inline constexpr Mat2x3 unitMat2x3() noexcept{
        return{
            Vec2{1.0f, 0.0f},
            Vec2{0.0f, 1.0f},
            Vec2{0.0f, 0.0f}
        };
    }

    inline constexpr Vec2 operator*(Mat2x3 lhs, Vec3 rhs) noexcept{
        return lhs[0]*rhs.x + lhs[1]*rhs.y + lhs[2]*rhs.z;
    }

    inline constexpr f32 det(Mat2x3 m) noexcept{
        return m[0].x*m[1].y - m[0].y*m[1].x;
    }

    Canvas2DRAII CreateImGuiCanvas(){
        return std::make_unique<ImGuiCanvas>();
    }

    class ImGuiCanvas::Impl{
    private:
        ImDrawList* drawList = nullptr;
        Mat2x3 transform = unitMat2x3();

    public:
        Impl() = default;

        void Begin();
        void End();

        void SetTransform(Mat2x3);

        void Line(Vec2 v0, Vec2 v1, const StrokeStyle&);
        void Polyline(std::span<const Vec2>, const StrokeStyle&);
        void Circle(Vec2 center, f32 r, const StrokeStyle&);
        void Quad(Vec2 lt, Vec2 rt, Vec2 rb, Vec2 lb, const StrokeStyle&);
        void Text(StrView utf8, Vec2 pos, const TextStyle&);

    private:
        ImVec2 apply(Vec2 v){
            return toImVec2(transform * toVec3(v, 1.0f));
        }
        f32 thickness(const StrokeStyle& style) const noexcept{
            auto uniformScale = std::sqrt(std::abs(det(transform)));

            return style.screenSpaceWidth ?
                style.width :
                style.width * uniformScale;
        }
    };

    ImGuiCanvas::ImGuiCanvas()
        : impl(std::make_unique<Impl>())
    {
        SMOL_ASSERT(ImGui::GetCurrentContext() != nullptr);
    }

    ImGuiCanvas::~ImGuiCanvas() = default;

    void ImGuiCanvas::Begin(RHISwapchain&){
        impl->Begin();
    }
    void ImGuiCanvas::Impl::Begin(){
        SMOL_ASSERT(drawList == nullptr);

        drawList = ImGui::GetBackgroundDrawList();
        transform = unitMat2x3();
    }

    void ImGuiCanvas::End(){
        impl->End();
    }
    void ImGuiCanvas::Impl::End(){
        SMOL_ASSERT(drawList != nullptr);

        drawList = nullptr;
    }

    void ImGuiCanvas::SetTransform(Transform2D t){
        const f32 cost = std::cos(t.theta);
        const f32 sint = std::sin(t.theta);

        impl->SetTransform(Mat2x3{
            Vec2{t.scale.x *  cost, t.scale.x * sint},
            Vec2{t.scale.y * -sint, t.scale.y * cost},
            Vec2{t.position.x, t.position.x},
        });
    }
    void ImGuiCanvas::Impl::SetTransform(Mat2x3 mat){
        transform = mat;
    }

    void ImGuiCanvas::Line(Vec2 v0, Vec2 v1, const StrokeStyle& style){
        impl->Line(v0, v1, style);
    }
    void ImGuiCanvas::Impl::Line(Vec2 v0, Vec2 v1, const StrokeStyle& style){
        SMOL_ASSERT(drawList != nullptr);

        drawList->AddLine(
            apply(v0),
            apply(v1),
            toImU32(style.color),
            thickness(style)
        );
    }

    void ImGuiCanvas::Polyline(std::span<const Vec2> points, const StrokeStyle& style){
        SMOL_ASSERT(points.size() >= 2);

        impl->Polyline(points, style);
    }
    void ImGuiCanvas::Impl::Polyline(std::span<const Vec2> points, const StrokeStyle& style){
        SMOL_ASSERT(drawList != nullptr);

        std::vector<ImVec2> screen;
        screen.reserve(points.size());
        for(auto p: points){
            screen.push_back(apply(p));
        }

        drawList->AddPolyline(
            screen.data(),
            static_cast<int>(screen.size()),
            toImU32(style.color),
            // open polyline
            ImDrawFlags_None,
            thickness(style)
        );
    }

    void ImGuiCanvas::Circle(Vec2 center, f32 r, const StrokeStyle& style){
        impl->Circle(center, r, style);
    }
    void ImGuiCanvas::Impl::Circle(Vec2 center, f32 r, const StrokeStyle& style){
        SMOL_ASSERT(drawList != nullptr);
        const auto uniformScale = std::sqrt(std::abs(det(transform)));

        drawList->AddCircle(
            apply(center),
            r * uniformScale,
            toImU32(style.color),
            // 0 = ImGui picks the segment count from the radius
            0,
            thickness(style)
        );
    }

    void ImGuiCanvas::Rect(Vec2 min, Vec2 max, const StrokeStyle& style){
        const auto lb = Vec2{min.x, max.y};
        const auto rt = Vec2{max.x, min.y};

        impl->Quad(min, rt, max, lb, style);
    }
    void ImGuiCanvas::Impl::Quad(Vec2 lt, Vec2 rt, Vec2 rb, Vec2 lb, const StrokeStyle& style){
        SMOL_ASSERT(drawList != nullptr);

        drawList->AddQuad(
            apply(lt), apply(rt), apply(rb), apply(lb),
            toImU32(style.color),
            thickness(style)
        );
    }

    void ImGuiCanvas::Text(StrView utf8, Vec2 pos, const TextStyle& style){
        impl->Text(utf8, pos, style);
    }
    void ImGuiCanvas::Impl::Text(StrView utf8, Vec2 pos, const TextStyle& style){
        SMOL_ASSERT(drawList != nullptr);

        drawList->AddText(
            ImGui::GetFont(),
            style.fontSize,
            apply(pos),
            toImU32(style.color),
            utf8.data(),
            utf8.data() + utf8.size(),
            style.size.x > 0.0f ?
                style.size.x : 0.0f
        );
    }
}
