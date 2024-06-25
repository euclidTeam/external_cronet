//! Graphics state for the TrueType interpreter.

mod projection;
mod round;
mod zone;

use core::ops::{Deref, DerefMut};
use read_fonts::types::Point;

pub use {
    round::{RoundMode, RoundState},
    zone::{Zone, ZonePointer},
};

/// Describes the axis to which a measurement or point movement operation
/// applies.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum CoordAxis {
    #[default]
    Both,
    X,
    Y,
}

/// Context in which instructions are executed.
///
/// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html>
#[derive(Debug)]
pub struct GraphicsState<'a> {
    /// Fields of the graphics state that persist between calls to the intepreter.
    pub retained: RetainedGraphicsState,
    /// A unit vector whose direction establishes an axis along which
    /// distances are measured.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#projection%20vector>
    pub proj_vector: Point<i32>,
    /// Current axis for the projection vector.
    pub proj_axis: CoordAxis,
    /// A second projection vector set to a line defined by the original
    /// outline location of two points. The dual projection vector is used
    /// when it is necessary to measure distances from the scaled outline
    /// before any instructions were executed.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#dual%20projection%20vector>
    pub dual_proj_vector: Point<i32>,
    /// Current axis for the dual projection vector.
    pub dual_proj_axis: CoordAxis,
    /// A unit vector that establishes an axis along which points can move.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#freedom%20vector>
    pub freedom_vector: Point<i32>,
    /// Current axis for point movement.
    pub freedom_axis: CoordAxis,
    /// Dot product of freedom and projection vectors.
    pub fdotp: i32,
    /// Determines the manner in which values are rounded.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#round%20state>
    pub round_state: RoundState,
    /// First reference point.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#rp0>
    pub rp0: usize,
    /// Second reference point.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#rp1>    
    pub rp1: usize,
    /// Third reference point.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#rp1>     
    pub rp2: usize,
    /// Makes it possible to repeat certain instructions a designated number of
    /// times. The default value of one assures that unless the value of loop
    /// is altered, these instructions will execute one time.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#loop>
    pub loop_counter: u32,
    /// First zone pointer.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#zp0>
    pub zp0: ZonePointer,
    /// Second zone pointer.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#zp1>
    pub zp1: ZonePointer,
    /// Third zone pointer.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#zp2>
    pub zp2: ZonePointer,
    /// Outline data for each zone.
    ///
    /// This array contains the twilight and glyph zones, in that order.
    pub zones: [Zone<'a>; 2],
    /// If true, enables a set of backward compabitility heuristics.
    /// Otherwise, the interpreter operates in "native ClearType mode".
    ///
    /// Defaults to true.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttinterp.h#L344>
    pub backward_compatibility: bool,
}

impl Default for GraphicsState<'_> {
    fn default() -> Self {
        // For table of default values, see <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_graphics_state>
        // All vectors are set to the x-axis (normalized in 2.14)
        let vector = Point::new(0x4000, 0);
        Self {
            retained: RetainedGraphicsState::default(),
            proj_vector: vector,
            proj_axis: CoordAxis::Both,
            dual_proj_vector: vector,
            dual_proj_axis: CoordAxis::Both,
            freedom_vector: vector,
            freedom_axis: CoordAxis::Both,
            fdotp: 0x4000,
            round_state: RoundState::default(),
            rp0: 0,
            rp1: 0,
            rp2: 0,
            loop_counter: 1,
            zp0: ZonePointer::default(),
            zp1: ZonePointer::default(),
            zp2: ZonePointer::default(),
            zones: [Zone::default(), Zone::default()],
            backward_compatibility: true,
        }
    }
}

/// The persistent graphics state.
///
/// Some of the graphics state is set by the control value program and
/// persists between runs of the interpreter. This struct captures that
/// state.
///
/// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html>
#[derive(Copy, Clone, Debug)]
pub struct RetainedGraphicsState {
    /// Controls whether the sign of control value table entries will be
    /// changed to match the sign of the actual distance measurement with
    /// which it is compared.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#auto%20flip>
    pub auto_flip: bool,
    /// Limits the regularizing effects of control value table entries to
    /// cases where the difference between the table value and the measurement
    /// taken from the original outline is sufficiently small.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#control_value_cut-in>
    pub control_value_cutin: i32,
    /// Establishes the base value used to calculate the range of point sizes
    /// to which a given DELTAC[] or DELTAP[] instruction will apply.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#delta%20base>
    pub delta_base: u16,
    /// Determines the range of movement and smallest magnitude of movement
    /// (the step) in a DELTAC[] or DELTAP[] instruction.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#delta%20shift>
    pub delta_shift: u16,
    /// Makes it possible to turn off instructions under some circumstances.
    /// When set to TRUE, no instructions will be executed
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#instruct%20control>
    pub instruct_control: u8,
    /// Establishes the smallest possible value to which a distance will be
    /// rounded.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#minimum%20distance>
    pub min_distance: i32,
    /// Determines whether the interpreter will activate dropout control for
    /// the current glyph.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#scan%20control>
    pub scan_control: bool,
    /// Type associated with `scan_control`.
    pub scan_type: i32,
    /// The distance difference below which the interpreter will replace a
    /// CVT distance or an actual distance in favor of the single width value.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#single_width_cut_in>
    pub single_width_cutin: i32,
    /// The value used in place of the control value table distance or the
    /// actual distance value when the difference between that distance and
    /// the single width value is less than the single width cut-in.
    ///
    /// See <https://developer.apple.com/fonts/TrueType-Reference-Manual/RM04/Chap4.html#single_width_value>
    pub single_width: i32,
    /// The scale factor for the current instance. Conversion from font units
    /// to 26.6 for current ppem.
    pub scale: i32,
    /// The nominal pixels per em value for the current instance.
    pub ppem: i32,
    /// True if a rotation is being applied.
    pub is_rotated: bool,
    /// True if a non-uniform scale is being applied.
    pub is_stretched: bool,
}

impl Default for RetainedGraphicsState {
    fn default() -> Self {
        // For table of default values, see <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_graphics_state>
        Self {
            auto_flip: true,
            // 17/16 pixels in 26.6
            // (17 * 64 / 16) = 68
            control_value_cutin: 68,
            delta_base: 9,
            delta_shift: 3,
            instruct_control: 0,
            // 1 pixel in 26.6
            min_distance: 64,
            scan_control: false,
            scan_type: 0,
            single_width_cutin: 0,
            single_width: 0,
            scale: 0,
            ppem: 0,
            is_rotated: false,
            is_stretched: false,
        }
    }
}

impl Deref for GraphicsState<'_> {
    type Target = RetainedGraphicsState;

    fn deref(&self) -> &Self::Target {
        &self.retained
    }
}

impl DerefMut for GraphicsState<'_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.retained
    }
}
