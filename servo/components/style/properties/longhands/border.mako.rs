/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import Keyword, Method, ALL_CORNERS, PHYSICAL_SIDES, ALL_SIDES, maybe_moz_logical_alias %>

<% data.new_style_struct("Border", inherited=False,
                   additional_methods=[Method("border_" + side + "_has_nonzero_width",
                                              "bool") for side in ["top", "right", "bottom", "left"]]) %>
<%
    def maybe_logical_spec(side, kind):
        if side[1]: # if it is logical
            return "https://drafts.csswg.org/css-logical-props/#propdef-border-%s-%s" % (side[0], kind)
        else:
            return "https://drafts.csswg.org/css-backgrounds/#border-%s-%s" % (side[0], kind)
%>
% for side in ALL_SIDES:
    <%
        side_name = side[0]
        is_logical = side[1]
    %>
    ${helpers.predefined_type(
        "border-%s-color" % side_name, "Color",
        "computed_value::T::currentcolor()",
        alias=maybe_moz_logical_alias(product, side, "-moz-border-%s-color"),
        spec=maybe_logical_spec(side, "color"),
        animation_value_type="AnimatedColor",
        logical=is_logical,
        logical_group="border-color",
        allow_quirks=not is_logical,
        flags="APPLIES_TO_FIRST_LETTER",
        ignored_when_colors_disabled=True,
    )}

    ${helpers.predefined_type(
        "border-%s-style" % side_name, "BorderStyle",
        "specified::BorderStyle::None",
        alias=maybe_moz_logical_alias(product, side, "-moz-border-%s-style"),
        spec=maybe_logical_spec(side, "style"),
        flags="APPLIES_TO_FIRST_LETTER",
        animation_value_type="discrete" if not is_logical else "none",
        logical=is_logical,
        logical_group="border-style",
        needs_context=False,
    )}

    ${helpers.predefined_type(
        "border-%s-width" % side_name,
        "BorderSideWidth",
        "crate::values::computed::NonNegativeLength::new(3.)",
        computed_type="crate::values::computed::NonNegativeLength",
        alias=maybe_moz_logical_alias(product, side, "-moz-border-%s-width"),
        spec=maybe_logical_spec(side, "width"),
        animation_value_type="NonNegativeLength",
        logical=is_logical,
        logical_group="border-width",
        flags="APPLIES_TO_FIRST_LETTER GETCS_NEEDS_LAYOUT_FLUSH",
        allow_quirks=not is_logical,
        servo_restyle_damage="reflow rebuild_and_reflow_inline"
    )}
% endfor

% for corner in ALL_CORNERS:
    <%
        corner_name = corner[0]
        is_logical = corner[1]
        if is_logical:
            prefixes = None
        else:
            prefixes = "webkit"
    %>
    ${helpers.predefined_type(
        "border-%s-radius" % corner_name,
        "BorderCornerRadius",
        "computed::BorderCornerRadius::zero()",
        "parse",
        extra_prefixes=prefixes,
        spec=maybe_logical_spec(corner, "radius"),
        boxed=True,
        flags="APPLIES_TO_FIRST_LETTER",
        animation_value_type="BorderCornerRadius",
        logical_group="border-radius",
        logical=is_logical,
    )}
% endfor

/// -moz-border-*-colors: color, string, enum, none, inherit/initial
/// These non-spec properties are just for Gecko (Stylo) internal use.
% for side in PHYSICAL_SIDES:
    <%helpers:longhand name="-moz-border-${side}-colors" animation_value_type="discrete"
                       spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-border-*-colors)"
                       products="gecko"
                       flags="APPLIES_TO_FIRST_LETTER"
                       ignored_when_colors_disabled="True">
        use std::fmt::{self, Write};
        use style_traits::{CssWriter, ToCss};
        use values::specified::RGBAColor;

        pub mod computed_value {
            use cssparser::RGBA;
            #[derive(Clone, Debug, MallocSizeOf, PartialEq, SpecifiedValueInfo)]
            pub struct T(pub Option<Vec<RGBA>>);
        }

        #[derive(Clone, Debug, MallocSizeOf, PartialEq, SpecifiedValueInfo)]
        pub enum SpecifiedValue {
            None,
            Colors(Vec<RGBAColor>),
        }

        impl ToCss for computed_value::T {
            fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: Write {
                match self.0 {
                    None => return dest.write_str("none"),
                    Some(ref vec) => {
                        let mut first = true;
                        for ref color in vec {
                            if !first {
                                dest.write_str(" ")?;
                            }
                            first = false;
                            color.to_css(dest)?
                        }
                        Ok(())
                    }
                }
            }
        }

        impl ToCss for SpecifiedValue {
            fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: Write {
                match *self {
                    SpecifiedValue::None => return dest.write_str("none"),
                    SpecifiedValue::Colors(ref vec) => {
                        let mut first = true;
                        for ref color in vec {
                            if !first {
                                dest.write_str(" ")?;
                            }
                            first = false;
                            color.to_css(dest)?
                        }
                        Ok(())
                    }
                }
            }
        }

        #[inline] pub fn get_initial_value() -> computed_value::T {
            computed_value::T(None)
        }

        #[inline] pub fn get_initial_specified_value() -> SpecifiedValue {
            SpecifiedValue::None
        }

        impl ToComputedValue for SpecifiedValue {
            type ComputedValue = computed_value::T;

            #[inline]
            fn to_computed_value(&self, context: &Context) -> computed_value::T {
                match *self {
                    SpecifiedValue::Colors(ref vec) => {
                        computed_value::T(Some(vec.iter()
                                                  .map(|c| c.to_computed_value(context))
                                                  .collect()))
                    },
                    SpecifiedValue::None => {
                        computed_value::T(None)
                    }
                }
            }
            #[inline]
            fn from_computed_value(computed: &computed_value::T) -> Self {
                match *computed {
                    computed_value::T(Some(ref vec)) => {
                        SpecifiedValue::Colors(vec.iter()
                                                  .map(ToComputedValue::from_computed_value)
                                                  .collect())
                    },
                    computed_value::T(None) => {
                        SpecifiedValue::None
                    }
                }
            }
        }

        #[inline]
        pub fn parse<'i, 't>(context: &ParserContext, input: &mut Parser<'i, 't>)
                             -> Result<SpecifiedValue, ParseError<'i>> {
            if input.try(|input| input.expect_ident_matching("none")).is_ok() {
                return Ok(SpecifiedValue::None)
            }

            let mut result = Vec::new();
            while let Ok(value) = input.try(|i| RGBAColor::parse(context, i)) {
                result.push(value);
            }

            if !result.is_empty() {
                Ok(SpecifiedValue::Colors(result))
            } else {
                Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError))
            }
        }
    </%helpers:longhand>
% endfor

${helpers.single_keyword(
    "box-decoration-break",
    "slice clone",
    gecko_enum_prefix="StyleBoxDecorationBreak",
    spec="https://drafts.csswg.org/css-break/#propdef-box-decoration-break",
    products="gecko",
    animation_value_type="discrete",
)}

${helpers.single_keyword(
    "-moz-float-edge",
    "content-box margin-box",
    gecko_ffi_name="mFloatEdge",
    gecko_enum_prefix="StyleFloatEdge",
    products="gecko",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-float-edge)",
    animation_value_type="discrete",
)}

${helpers.predefined_type(
    "border-image-source",
    "ImageLayer",
    initial_value="Either::First(None_)",
    initial_specified_value="Either::First(None_)",
    spec="https://drafts.csswg.org/css-backgrounds/#the-background-image",
    vector=False,
    animation_value_type="discrete",
    flags="APPLIES_TO_FIRST_LETTER",
    boxed=product == "servo",
    ignored_when_colors_disabled=True
)}

${helpers.predefined_type(
    "border-image-outset",
    "NonNegativeLengthOrNumberRect",
    initial_value="generics::rect::Rect::all(computed::NonNegativeLengthOrNumber::zero())",
    initial_specified_value="generics::rect::Rect::all(specified::NonNegativeLengthOrNumber::zero())",
    spec="https://drafts.csswg.org/css-backgrounds/#border-image-outset",
    animation_value_type="discrete",
    flags="APPLIES_TO_FIRST_LETTER",
    boxed=True,
)}

${helpers.predefined_type(
    "border-image-repeat",
    "BorderImageRepeat",
    "computed::BorderImageRepeat::stretch()",
    initial_specified_value="specified::BorderImageRepeat::stretch()",
    animation_value_type="discrete",
    spec="https://drafts.csswg.org/css-backgrounds/#the-border-image-repeat",
    flags="APPLIES_TO_FIRST_LETTER",
)}

${helpers.predefined_type(
    "border-image-width",
    "BorderImageWidth",
    initial_value="computed::BorderImageWidth::all(computed::BorderImageSideWidth::one())",
    initial_specified_value="specified::BorderImageWidth::all(specified::BorderImageSideWidth::one())",
    spec="https://drafts.csswg.org/css-backgrounds/#border-image-width",
    animation_value_type="discrete",
    flags="APPLIES_TO_FIRST_LETTER",
    boxed=True,
)}

${helpers.predefined_type(
    "border-image-slice",
    "BorderImageSlice",
    initial_value="computed::BorderImageSlice::hundred_percent()",
    initial_specified_value="specified::BorderImageSlice::hundred_percent()",
    spec="https://drafts.csswg.org/css-backgrounds/#border-image-slice",
    animation_value_type="discrete",
    flags="APPLIES_TO_FIRST_LETTER",
    boxed=True,
)}

// FIXME(emilio): Why does this live here? ;_;
#[cfg(feature = "gecko")]
impl crate::values::computed::BorderImageWidth {
    pub fn to_gecko_rect(&self, sides: &mut crate::gecko_bindings::structs::nsStyleSides) {
        use crate::gecko_bindings::sugar::ns_style_coord::{CoordDataMut, CoordDataValue};
        use crate::gecko::values::GeckoStyleCoordConvertible;
        use crate::values::generics::border::BorderImageSideWidth;

        % for i in range(0, 4):
        match self.${i} {
            BorderImageSideWidth::Auto => {
                sides.data_at_mut(${i}).set_value(CoordDataValue::Auto)
            },
            BorderImageSideWidth::Length(l) => {
                l.to_gecko_style_coord(&mut sides.data_at_mut(${i}))
            },
            BorderImageSideWidth::Number(n) => {
                sides.data_at_mut(${i}).set_value(CoordDataValue::Factor(n.0))
            },
        }
        % endfor
    }

    pub fn from_gecko_rect(
        sides: &crate::gecko_bindings::structs::nsStyleSides,
    ) -> Option<crate::values::computed::BorderImageWidth> {
        use crate::gecko_bindings::structs::nsStyleUnit::{eStyleUnit_Factor, eStyleUnit_Auto};
        use crate::gecko_bindings::sugar::ns_style_coord::CoordData;
        use crate::gecko::values::GeckoStyleCoordConvertible;
        use crate::values::computed::{LengthPercentage, Number};
        use crate::values::generics::border::BorderImageSideWidth;
        use crate::values::generics::NonNegative;

        Some(
            crate::values::computed::BorderImageWidth::new(
                % for i in range(0, 4):
                match sides.data_at(${i}).unit() {
                    eStyleUnit_Auto => {
                        BorderImageSideWidth::Auto
                    },
                    eStyleUnit_Factor => {
                        BorderImageSideWidth::Number(
                            NonNegative(Number::from_gecko_style_coord(&sides.data_at(${i}))
                                .expect("sides[${i}] could not convert to Number")))
                    },
                    _ => {
                        BorderImageSideWidth::Length(
                            NonNegative(LengthPercentage::from_gecko_style_coord(&sides.data_at(${i}))
                                .expect("sides[${i}] could not convert to LengthPercentage")))
                    },
                },
                % endfor
            )
        )
    }
}
