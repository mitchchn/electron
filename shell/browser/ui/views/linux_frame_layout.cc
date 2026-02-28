// Copyright (c) 2025 Mitchell Cohen.
// Copyright 2022 The Chromium Authors.
// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/views/linux_frame_layout.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "shell/browser/linux/x11_util.h"
#include "shell/browser/native_window_views.h"
#include "shell/browser/ui/electron_desktop_window_tree_host_linux.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_background.h"

namespace electron {

namespace {
// This should match Chromium's value.
constexpr int kResizeBorder = 10;
// This should match FramelessView's inside resize band.
constexpr int kResizeInsideBoundsSize = 5;

// These should match Chromium's restored frame edge thickness.
constexpr gfx::Insets kDefaultCustomFrameBorder = gfx::Insets::TLBR(2, 1, 1, 1);

constexpr int kBorderAlpha = 0x26;
}  // namespace

// static
std::unique_ptr<LinuxFrameLayout> LinuxFrameLayout::Create(
    NativeWindowViews* window,
    bool wants_shadow,
    CSDStyle csd_style) {
  if (x11_util::IsX11() || window->IsTranslucent() || !wants_shadow) {
    return std::make_unique<LinuxUndecoratedFrameLayout>(window);
  } else if (csd_style == CSDStyle::kCustom) {
    return std::make_unique<LinuxCSDCustomFrameLayout>(window);
  } else {
    return std::make_unique<LinuxCSDNativeFrameLayout>(window);
  }
}

gfx::Insets LinuxFrameLayout::GetResizeBorderInsets() const {
  gfx::Insets insets = RestoredFrameBorderInsets();
  return insets.IsEmpty() ? GetInputInsets() : insets;
}

LinuxCSDBaseLayout::LinuxCSDBaseLayout(NativeWindowViews* window)
    : window_(window) {
  host_supports_client_frame_shadow_ = SupportsClientFrameShadow();
}

gfx::Insets LinuxCSDBaseLayout::GetInputInsets() const {
  bool showing_shadow = IsShowingShadow();
  return gfx::Insets(showing_shadow ? kResizeBorder : 0);
}

bool LinuxCSDBaseLayout::IsShowingShadow() const {
  return host_supports_client_frame_shadow_ && !window_->IsMaximized() &&
         !window_->IsFullscreen();
}

bool LinuxCSDBaseLayout::SupportsClientFrameShadow() const {
  auto* tree_host = static_cast<ElectronDesktopWindowTreeHostLinux*>(
      ElectronDesktopWindowTreeHostLinux::GetHostForWidget(
          window_->GetAcceleratedWidget()));
  if (!tree_host) {
    return false;
  }
  return tree_host->SupportsClientFrameShadow();
}

bool LinuxCSDBaseLayout::tiled() const {
  return tiled_;
}

void LinuxCSDBaseLayout::set_tiled(bool tiled) {
  tiled_ = tiled;
}

gfx::Rect LinuxCSDBaseLayout::GetWindowBounds() const {
  gfx::Rect bounds = window_->widget()->GetWindowBoundsInScreen();
  bounds.Inset(RestoredFrameBorderInsets());
  return bounds;
}

int LinuxCSDBaseLayout::GetTranslucentTopAreaHeight() const {
  return 0;
}

gfx::Insets LinuxCSDBaseLayout::NormalizeBorderInsets(
    const gfx::Insets& frame_insets,
    const gfx::Insets& input_insets) const {
  auto expand_if_visible = [](int side_thickness, int min_band) {
    return side_thickness > 0 ? std::max(side_thickness, min_band) : 0;
  };

  // Ensure hit testing for resize targets works
  // even if borders/shadows are absent on some edges.
  gfx::Insets merged;
  merged.set_top(expand_if_visible(frame_insets.top(), input_insets.top()));
  merged.set_left(expand_if_visible(frame_insets.left(), input_insets.left()));
  merged.set_bottom(
      expand_if_visible(frame_insets.bottom(), input_insets.bottom()));
  merged.set_right(
      expand_if_visible(frame_insets.right(), input_insets.right()));

  return base::i18n::IsRTL() ? gfx::Insets::TLBR(merged.top(), merged.right(),
                                                 merged.bottom(), merged.left())
                             : merged;
}

LinuxCSDNativeFrameLayout::LinuxCSDNativeFrameLayout(NativeWindowViews* window)
    : LinuxCSDBaseLayout(window) {}

gfx::Insets LinuxCSDNativeFrameLayout::RestoredFrameBorderInsets() const {
  const gfx::Insets input_insets = GetInputInsets();
  const gfx::Insets frame_insets = GetFrameProvider()->GetFrameThicknessDip();
  return NormalizeBorderInsets(frame_insets, input_insets);
}

SkRRect LinuxCSDNativeFrameLayout::GetRoundedWindowBounds() const {
  SkRect rect = gfx::RectToSkRect(GetWindowBounds());
  SkRRect rrect;

  if (!window_->IsMaximized()) {
    float radius = GetFrameProvider()->GetTopCornerRadiusDip();
    SkPoint round_point{radius, radius};
    SkPoint radii[] = {round_point, round_point, {}, {}};
    rrect.setRectRadii(rect, radii);
  } else {
    rrect.setRect(rect);
  }
  return rrect;
}

ui::WindowFrameProvider* LinuxCSDNativeFrameLayout::GetFrameProvider() const {
  return ui::LinuxUiTheme::GetForProfile(nullptr)->GetWindowFrameProvider(
      !host_supports_client_frame_shadow_, tiled(), window_->IsMaximized());
}

LinuxCSDCustomFrameLayout::LinuxCSDCustomFrameLayout(NativeWindowViews* window)
    : LinuxCSDBaseLayout(window) {}

gfx::Insets LinuxCSDCustomFrameLayout::RestoredFrameBorderInsets() const {
  const gfx::Insets input_insets = GetInputInsets();
  const bool showing_shadow = IsShowingShadow();
  gfx::Insets frame_insets = kDefaultCustomFrameBorder;
  if (showing_shadow) {
    const auto shadow_values = tiled()
                                   ? gfx::ShadowValues()
                                   : GetFrameShadowValuesLinux(/*active=*/true);

    // The border must be at least as large as the shadow.
    gfx::Rect frame_extents;
    for (const auto& shadow_value : shadow_values) {
      const auto shadow_radius = shadow_value.blur() / 4;
      const gfx::InsetsF shadow_insets(shadow_radius);
      gfx::RectF shadow_extents;
      shadow_extents.Inset(-shadow_insets);
      shadow_extents.set_origin(shadow_extents.origin() +
                                shadow_value.offset());
      frame_extents.Union(gfx::ToEnclosingRect(shadow_extents));
    }

    // The border must be at least as large as the input region.
    gfx::Rect input_extents;
    input_extents.Inset(-input_insets);
    frame_extents.Union(input_extents);

    frame_insets =
        gfx::Insets::TLBR(-frame_extents.y(), -frame_extents.x(),
                          frame_extents.bottom(), frame_extents.right());
  } else {
    frame_insets.set_top(0);
  }

  return NormalizeBorderInsets(frame_insets, input_insets);
}

SkRRect LinuxCSDCustomFrameLayout::GetRoundedWindowBounds() const {
  SkRRect rrect;
  // OpaqueFrameView currently paints square top corners.
  rrect.setRect(gfx::RectToSkRect(GetWindowBounds()));
  return rrect;
}

LinuxUndecoratedFrameLayout::LinuxUndecoratedFrameLayout(
    NativeWindowViews* window)
    : window_(window) {}

gfx::Insets LinuxUndecoratedFrameLayout::RestoredFrameBorderInsets() const {
  return gfx::Insets();
}

gfx::Insets LinuxUndecoratedFrameLayout::GetInputInsets() const {
  return gfx::Insets(kResizeInsideBoundsSize);
}

bool LinuxUndecoratedFrameLayout::IsShowingShadow() const {
  return false;
}

bool LinuxUndecoratedFrameLayout::SupportsClientFrameShadow() const {
  return false;
}

bool LinuxUndecoratedFrameLayout::tiled() const {
  return tiled_;
}

void LinuxUndecoratedFrameLayout::set_tiled(bool tiled) {
  tiled_ = tiled;
}

gfx::Rect LinuxUndecoratedFrameLayout::GetWindowBounds() const {
  // With no transparent insets, widget bounds and logical bounds match.
  return window_->widget()->GetWindowBoundsInScreen();
}

SkRRect LinuxUndecoratedFrameLayout::GetRoundedWindowBounds() const {
  SkRRect rrect;
  rrect.setRect(gfx::RectToSkRect(GetWindowBounds()));
  return rrect;
}

int LinuxUndecoratedFrameLayout::GetTranslucentTopAreaHeight() const {
  return 0;
}

gfx::ShadowValues GetFrameShadowValuesLinux(bool active) {
  const int elevation = views::LayoutProvider::Get()->GetShadowElevationMetric(
      active ? views::Emphasis::kMaximum : views::Emphasis::kMedium);
  return gfx::ShadowValue::MakeMdShadowValues(elevation);
}

void PaintRestoredFrameBorderLinux(gfx::Canvas& canvas,
                                   const views::View& view,
                                   views::FrameBackground* frame_background,
                                   const SkRRect& clip,
                                   bool showing_shadow,
                                   bool is_active,
                                   const gfx::Insets& border,
                                   const gfx::ShadowValues& shadow_values,
                                   bool tiled) {
  const auto* color_provider = view.GetColorProvider();
  if (frame_background) {
    gfx::ScopedCanvas scoped_canvas(&canvas);
    canvas.sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
    const auto shadow_inset = showing_shadow ? border : gfx::Insets();
    frame_background->PaintMaximized(
        &canvas, view.GetNativeTheme(), color_provider, shadow_inset.left(),
        shadow_inset.top(), view.width() - shadow_inset.width());
    if (!showing_shadow) {
      frame_background->FillFrameBorders(&canvas, &view, border.left(),
                                         border.right(), border.bottom());
    }
  }

  const SkScalar one_pixel = SkFloatToScalar(1 / canvas.image_scale());
  SkRRect outset_rect = clip;
  SkRRect inset_rect = clip;
  if (tiled) {
    outset_rect.outset(1, 1);
  } else if (showing_shadow) {
    outset_rect.outset(one_pixel, one_pixel);
  } else {
    inset_rect.inset(one_pixel, one_pixel);
  }

  cc::PaintFlags flags;
  const SkColor frame_color = color_provider->GetColor(
      is_active ? ui::kColorFrameActive : ui::kColorFrameInactive);
  const SkColor border_color =
      showing_shadow ? SK_ColorBLACK
                     : color_utils::PickContrastingColor(
                           SK_ColorBLACK, SK_ColorWHITE, frame_color);
  flags.setColor(SkColorSetA(border_color, kBorderAlpha));
  flags.setAntiAlias(true);
  if (showing_shadow) {
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values));
  }

  gfx::ScopedCanvas scoped_canvas(&canvas);
  canvas.sk_canvas()->clipRRect(inset_rect, SkClipOp::kDifference, true);
  canvas.sk_canvas()->drawRRect(outset_rect, flags);
}

}  // namespace electron
