/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawing.h"

#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsContainerFrame.h"
#include "nsDeviceContext.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsScrollbarFrame.h"
#include "nsNativeTheme.h"

using namespace mozilla::gfx;

namespace mozilla::widget {

using mozilla::RelativeLuminanceUtils;

/* static */
auto ScrollbarDrawing::GetDPIRatioForScrollbarPart(const nsPresContext* aPc)
    -> DPIRatio {
  DPIRatio ratio(
      float(AppUnitsPerCSSPixel()) /
      float(aPc->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom()));
  if (aPc->IsPrintPreview()) {
    ratio.scale *= aPc->GetPrintPreviewScaleForSequenceFrameOrScrollbars();
  }
  if (mKind == Kind::Cocoa) {
    return DPIRatio(ratio.scale >= 2.0f ? 2.0f : 1.0f);
  }
  return ratio;
}

/*static*/
nsScrollbarFrame* ScrollbarDrawing::GetParentScrollbarFrame(nsIFrame* aFrame) {
  for (; aFrame; aFrame = aFrame->GetParent()) {
    if (nsScrollbarFrame* f = do_QueryFrame(aFrame)) {
      return f;
    }
  }
  return nullptr;
}

/*static*/
bool ScrollbarDrawing::IsParentScrollbarRolledOver(nsIFrame* aFrame) {
  if (nsScrollbarFrame* f = GetParentScrollbarFrame(aFrame)) {
    if (f->PresContext()->UseOverlayScrollbars()) {
      return f->HasBeenHovered();
    }
    return f->GetContent()->AsElement()->State().HasState(ElementState::HOVER);
  }
  return false;
}

/*static*/
bool ScrollbarDrawing::IsParentScrollbarHoveredOrActive(nsIFrame* aFrame) {
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return scrollbarFrame &&
         scrollbarFrame->GetContent()
             ->AsElement()
             ->State()
             .HasAtLeastOneOfStates(ElementState::HOVER | ElementState::ACTIVE);
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(const ComputedStyle& aStyle) {
  auto scrollbarWidth = aStyle.StyleUIReset()->ScrollbarWidth();
  return scrollbarWidth == StyleScrollbarWidth::Thin;
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(nsIFrame* aFrame) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  return IsScrollbarWidthThin(*style);
}

CSSIntCoord ScrollbarDrawing::GetCSSScrollbarSize(StyleScrollbarWidth aWidth,
                                                  Overlay aOverlay) const {
  return mScrollbarSize[aWidth == StyleScrollbarWidth::Thin]
                       [aOverlay == Overlay::Yes];
}

void ScrollbarDrawing::ConfigureScrollbarSize(StyleScrollbarWidth aWidth,
                                              Overlay aOverlay,
                                              CSSIntCoord aSize) {
  mScrollbarSize[aWidth == StyleScrollbarWidth::Thin]
                [aOverlay == Overlay::Yes] = aSize;
}

void ScrollbarDrawing::ConfigureScrollbarSize(CSSIntCoord aSize) {
  ConfigureScrollbarSize(StyleScrollbarWidth::Auto, Overlay::No, aSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Auto, Overlay::Yes, aSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::No, aSize / 2);
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::Yes, aSize / 2);
}

LayoutDeviceIntCoord ScrollbarDrawing::GetScrollbarSize(
    const nsPresContext* aPresContext, StyleScrollbarWidth aWidth,
    Overlay aOverlay) {
  return (CSSCoord(GetCSSScrollbarSize(aWidth, aOverlay)) *
          GetDPIRatioForScrollbarPart(aPresContext))
      .Rounded();
}

LayoutDeviceIntCoord ScrollbarDrawing::GetScrollbarSize(
    const nsPresContext* aPresContext, nsIFrame* aFrame) {
  auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  auto width = style->StyleUIReset()->ScrollbarWidth();
  auto overlay =
      aPresContext->UseOverlayScrollbars() ? Overlay::Yes : Overlay::No;
  return GetScrollbarSize(aPresContext, width, overlay);
}

bool ScrollbarDrawing::IsScrollbarTrackOpaque(nsIFrame* aFrame) {
  auto trackColor = ComputeScrollbarTrackColor(
      aFrame, *nsLayoutUtils::StyleForScrollbar(aFrame),
      aFrame->PresContext()->Document()->State(),
      Colors(aFrame, StyleAppearance::ScrollbarVertical));
  return trackColor.a == 1.0f;
}

sRGBColor ScrollbarDrawing::ComputeScrollbarTrackColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.System(StyleSystemColor::Window);
  }
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(
        ui->mScrollbarColor.AsColors().track.CalcColor(aStyle));
  }
  static constexpr sRGBColor sDefaultDarkTrackColor =
      sRGBColor::FromU8(20, 20, 25, 77);
  static constexpr sRGBColor sDefaultTrackColor(
      gfx::sRGBColor::UnusualFromARGB(0xfff0f0f0));

  auto systemColor = aDocumentState.HasAllStates(DocumentState::WINDOW_INACTIVE)
                         ? StyleSystemColor::ThemedScrollbarInactive
                         : StyleSystemColor::ThemedScrollbar;
  return aColors.SystemOrElse(systemColor, [&] {
    return aColors.IsDark() ? sDefaultDarkTrackColor : sDefaultTrackColor;
  });
}

// Don't use the theme color for dark scrollbars if it's not a color (if it's
// grey-ish), as that'd either lack enough contrast, or be close to what we'd do
// by default anyways.
sRGBColor ScrollbarDrawing::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle), aElementState));
  }

  auto systemColor = [&] {
    if (aDocumentState.HasState(DocumentState::WINDOW_INACTIVE)) {
      return StyleSystemColor::ThemedScrollbarThumbInactive;
    }
    if (aElementState.HasState(ElementState::ACTIVE)) {
      if (aColors.HighContrast()) {
        return StyleSystemColor::Selecteditem;
      }
      return StyleSystemColor::ThemedScrollbarThumbActive;
    }
    if (aElementState.HasState(ElementState::HOVER)) {
      if (aColors.HighContrast()) {
        return StyleSystemColor::Selecteditem;
      }
      return StyleSystemColor::ThemedScrollbarThumbHover;
    }
    if (aColors.HighContrast()) {
      return StyleSystemColor::Windowtext;
    }
    return StyleSystemColor::ThemedScrollbarThumb;
  }();

  return aColors.SystemOrElse(systemColor, [&] {
    const nscolor unthemedColor = aColors.IsDark() ? NS_RGBA(249, 249, 250, 102)
                                                   : NS_RGB(0xcd, 0xcd, 0xcd);

    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        unthemedColor, aElementState));
  });
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollbar(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  const bool overlay = aFrame->PresContext()->UseOverlayScrollbars();
  if (overlay && !aElementState.HasAtLeastOneOfStates(ElementState::HOVER |
                                                      ElementState::ACTIVE)) {
    return true;
  }
  const auto color =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  if (overlay && mKind == Kind::Win11 &&
      StaticPrefs::widget_non_native_theme_win11_scrollbar_round_track()) {
    LayoutDeviceCoord radius =
        (aScrollbarKind == ScrollbarKind::Horizontal ? aRect.height
                                                     : aRect.width) /
        2.0f;
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, color,
                                             sRGBColor(), 0, radius / aDpiRatio,
                                             aDpiRatio);
  } else {
    ThemeDrawing::FillRect(aPaintData, aRect, color);
  }
  return true;
}

bool ScrollbarDrawing::PaintScrollbar(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aDrawTarget, aRect, aScrollbarKind, aFrame,
                                 aStyle, aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollbar(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                                 aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  auto scrollbarColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  ThemeDrawing::FillRect(aPaintData, aRect, scrollbarColor);
  return true;
}

bool ScrollbarDrawing::PaintScrollCorner(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aDrawTarget, aRect, aScrollbarKind, aFrame,
                                    aStyle, aDocumentState, aColors, aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollCorner(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aWrData, aRect, aScrollbarKind, aFrame,
                                    aStyle, aDocumentState, aColors, aDpiRatio);
}

nscolor ScrollbarDrawing::GetScrollbarButtonColor(nscolor aTrackColor,
                                                  ElementState aStates) {
  // See numbers in GetScrollbarArrowColor.
  // This function is written based on ratios between values listed there.

  bool isActive = aStates.HasState(ElementState::ACTIVE);
  bool isHover = aStates.HasState(ElementState::HOVER);
  if (!isActive && !isHover) {
    return aTrackColor;
  }
  float luminance = RelativeLuminanceUtils::Compute(aTrackColor);
  if (isActive) {
    if (luminance >= 0.18f) {
      luminance *= 0.134f;
    } else {
      luminance /= 0.134f;
      luminance = std::min(luminance, 1.0f);
    }
  } else {
    if (luminance >= 0.18f) {
      luminance *= 0.805f;
    } else {
      luminance /= 0.805f;
    }
  }
  return RelativeLuminanceUtils::Adjust(aTrackColor, luminance);
}

Maybe<nscolor> ScrollbarDrawing::GetScrollbarArrowColor(nscolor aButtonColor) {
  // In Windows 10 scrollbar, there are several gray colors used:
  //
  // State  | Background (lum) | Arrow   | Contrast
  // -------+------------------+---------+---------
  // Normal | Gray 240 (87.1%) | Gray 96 |     5.5
  // Hover  | Gray 218 (70.1%) | Black   |    15.0
  // Active | Gray 96  (11.7%) | White   |     6.3
  //
  // Contrast value is computed based on the definition in
  // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
  //
  // This function is written based on these values.

  if (NS_GET_A(aButtonColor) == 0) {
    // If the button color is transparent, because of e.g.
    // scrollbar-color: <something> transparent, then use
    // the thumb color, which is expected to have enough
    // contrast.
    return Nothing();
  }

  float luminance = RelativeLuminanceUtils::Compute(aButtonColor);
  // Color with luminance larger than 0.72 has contrast ratio over 4.6
  // to color with luminance of gray 96, so this value is chosen for
  // this range. It is the luminance of gray 221.
  if (luminance >= 0.72) {
    // ComputeRelativeLuminanceFromComponents(96). That function cannot
    // be constexpr because of std::pow.
    const float GRAY96_LUMINANCE = 0.117f;
    return Some(RelativeLuminanceUtils::Adjust(aButtonColor, GRAY96_LUMINANCE));
  }
  // The contrast ratio of a color to black equals that to white when its
  // luminance is around 0.18, with a contrast ratio ~4.6 to both sides,
  // thus the value below. It's the lumanince of gray 118.
  //
  // TODO(emilio): Maybe the button alpha is not the best thing to use here and
  // we should use the thumb alpha? It seems weird that the color of the arrow
  // depends on the opacity of the scrollbar thumb...
  if (luminance >= 0.18) {
    return Some(NS_RGBA(0, 0, 0, NS_GET_A(aButtonColor)));
  }
  return Some(NS_RGBA(255, 255, 255, NS_GET_A(aButtonColor)));
}

std::pair<sRGBColor, sRGBColor> ScrollbarDrawing::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    if (aElementState.HasAtLeastOneOfStates(ElementState::ACTIVE |
                                            ElementState::HOVER)) {
      return aColors.SystemPair(StyleSystemColor::Selecteditem,
                                StyleSystemColor::Buttonface);
    }
    return aColors.SystemPair(StyleSystemColor::Window,
                              StyleSystemColor::Windowtext);
  }

  auto trackColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  nscolor buttonColor =
      GetScrollbarButtonColor(trackColor.ToABGR(), aElementState);
  auto arrowColor =
      GetScrollbarArrowColor(buttonColor)
          .map(sRGBColor::FromABGR)
          .valueOrFrom([&] {
            return ComputeScrollbarThumbColor(aFrame, aStyle, aElementState,
                                              aDocumentState, aColors);
          });
  return {sRGBColor::FromABGR(buttonColor), arrowColor};
}

bool ScrollbarDrawing::PaintScrollbarButton(
    DrawTarget& aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, ScrollbarKind aScrollbarKind,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio&) {
  auto [buttonColor, arrowColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  // Scrollbar thumb and button are two CSS pixels thinner than the track.
  LayoutDeviceRect buttonRect(aRect);
  gfxFloat p2a = gfxFloat(aFrame->PresContext()->AppUnitsPerDevPixel());
  gfxFloat dev2css = round(AppUnitsPerCSSPixel() / p2a);
  const bool horizontal = aScrollbarKind == ScrollbarKind::Horizontal;
  if (horizontal) {
    buttonRect.Deflate(0, dev2css);
  } else {
    buttonRect.Deflate(dev2css, 0);
  }

  aDrawTarget.FillRect(buttonRect.ToUnknownRect(),
                       ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {5.0, 8.5, 12.0, 12.0, 8.5, 5.0};
  float arrowPolygonY[] = {9.0, 6.0, 9.0, 12.0, 9.0, 12.0};

  const float kPolygonSize = 17;

  const int32_t arrowNumPoints = std::size(arrowPolygonX);
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
      break;
    case StyleAppearance::ScrollbarbuttonDown:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowPolygonY[i] = kPolygonSize - arrowPolygonY[i];
      }
      break;
    case StyleAppearance::ScrollbarbuttonLeft:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        float temp = arrowPolygonX[i];
        arrowPolygonX[i] = arrowPolygonY[i];
        arrowPolygonY[i] = temp;
      }
      break;
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        float temp = arrowPolygonX[i];
        arrowPolygonX[i] = kPolygonSize - arrowPolygonY[i];
        arrowPolygonY[i] = temp;
      }
      break;
    default:
      return false;
  }

  // Compute the path and draw the scrollbar.
  const float scale = ThemeDrawing::ScaleToFillRect(aRect, kPolygonSize);
  RefPtr<gfx::PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  gfx::Point start =
      gfx::Point(aRect.X(), aRect.Y());
  gfx::Point p =
      start + gfx::Point(arrowPolygonX[0] * scale, arrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < arrowNumPoints; i++) {
    p = start +
        gfx::Point(arrowPolygonX[i] * scale, arrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<gfx::Path> path = builder->Finish();

  // The arrow should be drawn without antialiasing.
  DrawOptions arrowOptions(
    1.0f, gfx::CompositionOp::OP_OVER, gfx::AntialiasMode::NONE
  );

  aDrawTarget.Fill(path, gfx::ColorPattern(ToDeviceColor(arrowColor)), arrowOptions);
  return true;
}

}  // namespace mozilla::widget
