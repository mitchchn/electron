// Copyright (c) 2025 Mitchell Cohen.
// Copyright 2022 The Chromium Authors.
// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_UI_VIEWS_LINUX_FRAME_LAYOUT_H_
#define ELECTRON_SHELL_BROWSER_UI_VIEWS_LINUX_FRAME_LAYOUT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/shadow_value.h"
#include "ui/linux/window_frame_provider.h"

namespace gfx {
class Canvas;
class Insets;
class Rect;
}  // namespace gfx

namespace views {
class FrameBackground;
class View;
}  // namespace views

namespace electron {

class NativeWindowViews;

// Shared helper for CSD layout on Linux (shadows, resize regions, titlebars,
// etc.). Also helps views determine insets and perform
// bounds conversions between widget and logical coordinates.
class LinuxFrameLayout {
 public:
  enum class CSDStyle {
    kNativeFrame,
    kCustom,
  };

  virtual ~LinuxFrameLayout() = default;

  static std::unique_ptr<LinuxFrameLayout> Create(NativeWindowViews* window,
                                                  bool wants_shadow,
                                                  CSDStyle csd_style);

  // Insets from the transparent widget border to the opaque part of the window
  virtual gfx::Insets RestoredFrameBorderInsets() const = 0;
  // Insets for parts of the surface that should be counted for user input
  virtual gfx::Insets GetInputInsets() const = 0;
  // Insets to use for non-client resize hit-testing.
  gfx::Insets GetResizeBorderInsets() const;

  virtual bool IsShowingShadow() const = 0;
  virtual bool SupportsClientFrameShadow() const = 0;

  virtual bool tiled() const = 0;
  virtual void set_tiled(bool tiled) = 0;

  // The logical bounds of the window interior.
  virtual gfx::Rect GetWindowBounds() const = 0;
  // The logical window bounds as a rounded rect with corner radii applied.
  virtual SkRRect GetRoundedWindowBounds() const = 0;

  virtual int GetTranslucentTopAreaHeight() const = 0;
};

class LinuxCSDBaseLayout : public LinuxFrameLayout {
 public:
  explicit LinuxCSDBaseLayout(NativeWindowViews* window);
  ~LinuxCSDBaseLayout() override = default;

  gfx::Insets GetInputInsets() const override;
  bool IsShowingShadow() const override;
  bool SupportsClientFrameShadow() const override;
  bool tiled() const override;
  void set_tiled(bool tiled) override;
  gfx::Rect GetWindowBounds() const override;
  int GetTranslucentTopAreaHeight() const override;

 protected:
  gfx::Insets NormalizeBorderInsets(const gfx::Insets& frame_insets,
                                    const gfx::Insets& input_insets) const;

  raw_ptr<NativeWindowViews> window_;
  bool tiled_ = false;
  bool host_supports_client_frame_shadow_ = false;
};

// CSD strategy that uses the GTK window frame provider for metrics.
class LinuxCSDNativeFrameLayout : public LinuxCSDBaseLayout {
 public:
  explicit LinuxCSDNativeFrameLayout(NativeWindowViews* window);
  ~LinuxCSDNativeFrameLayout() override = default;

  gfx::Insets RestoredFrameBorderInsets() const override;
  SkRRect GetRoundedWindowBounds() const override;
  ui::WindowFrameProvider* GetFrameProvider() const;
};

// CSD strategy that uses custom metrics, similar to those used in Chromium.
class LinuxCSDCustomFrameLayout : public LinuxCSDBaseLayout {
 public:
  explicit LinuxCSDCustomFrameLayout(NativeWindowViews* window);
  ~LinuxCSDCustomFrameLayout() override = default;

  gfx::Insets RestoredFrameBorderInsets() const override;
  SkRRect GetRoundedWindowBounds() const override;
};

// No-decoration Linux frame layout implementation.
//
// Intended for cases where we do not allocate a transparent inset area around
// the window (e.g. X11 / server-side decorations, or when insets are disabled).
// All inset math returns 0.
class LinuxUndecoratedFrameLayout : public LinuxFrameLayout {
 public:
  explicit LinuxUndecoratedFrameLayout(NativeWindowViews* window);
  ~LinuxUndecoratedFrameLayout() override = default;

  gfx::Insets RestoredFrameBorderInsets() const override;
  gfx::Insets GetInputInsets() const override;
  bool IsShowingShadow() const override;
  bool SupportsClientFrameShadow() const override;
  bool tiled() const override;
  void set_tiled(bool tiled) override;
  gfx::Rect GetWindowBounds() const override;
  SkRRect GetRoundedWindowBounds() const override;
  int GetTranslucentTopAreaHeight() const override;

 private:
  raw_ptr<NativeWindowViews> window_;
  bool tiled_ = false;
};

gfx::ShadowValues GetFrameShadowValuesLinux(bool active);

void PaintRestoredFrameBorderLinux(gfx::Canvas& canvas,
                                   const views::View& view,
                                   views::FrameBackground* frame_background,
                                   const SkRRect& clip,
                                   bool showing_shadow,
                                   bool is_active,
                                   const gfx::Insets& border,
                                   const gfx::ShadowValues& shadow_values,
                                   bool tiled);
}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_UI_VIEWS_LINUX_FRAME_LAYOUT_H_
