//! Common bitmap (EBLC/EBDT/CBLC/CBDT) types.

include!("../../generated/generated_bitmap.rs");

impl BitmapSize {
    /// Returns the bitmap location information for the given glyph.
    ///
    /// The `offset_data` parameter is provided by the `offset_data()` method
    /// of the parent `Eblc` or `Cblc` table.
    ///
    /// The resulting [`BitmapLocation`] value is used by the `data()` method
    /// in the associated `Ebdt` or `Cbdt` table to extract the bitmap data.
    pub fn location(
        &self,
        offset_data: FontData,
        glyph_id: GlyphId,
    ) -> Result<BitmapLocation, ReadError> {
        if !(self.start_glyph_index()..=self.end_glyph_index()).contains(&glyph_id) {
            return Err(ReadError::OutOfBounds);
        }
        let mut location = BitmapLocation {
            bit_depth: self.bit_depth,
            ..BitmapLocation::default()
        };
        for ix in 0..self.number_of_index_subtables() {
            let subtable = self.subtable(offset_data, ix)?;
            if !(subtable.first_glyph_index..=subtable.last_glyph_index).contains(&glyph_id) {
                continue;
            }
            // glyph index relative to the first glyph in the subtable
            let glyph_ix =
                glyph_id.to_u16() as usize - subtable.first_glyph_index.to_u16() as usize;
            match &subtable.kind {
                IndexSubtable::Format1(st) => {
                    location.format = st.image_format();
                    location.data_offset = st.image_data_offset() as usize
                        + st.sbit_offsets()
                            .get(glyph_ix)
                            .ok_or(ReadError::OutOfBounds)?
                            .get() as usize;
                }
                IndexSubtable::Format2(st) => {
                    location.format = st.image_format();
                    let data_size = st.image_size() as usize;
                    location.data_size = Some(data_size);
                    location.data_offset = st.image_data_offset() as usize + glyph_ix * data_size;
                    location.metrics = Some(st.big_metrics()[0].clone());
                }
                IndexSubtable::Format3(st) => {
                    location.format = st.image_format();
                    location.data_offset = st.image_data_offset() as usize
                        + st.sbit_offsets()
                            .get(glyph_ix)
                            .ok_or(ReadError::OutOfBounds)?
                            .get() as usize;
                }
                IndexSubtable::Format4(st) => {
                    location.format = st.image_format();
                    let array = st.glyph_array();
                    let array_ix = match array.binary_search_by(|x| x.glyph_id().cmp(&glyph_id)) {
                        Ok(ix) => ix,
                        _ => {
                            return Err(ReadError::InvalidCollectionIndex(glyph_id.to_u16() as u32))
                        }
                    };
                    let offset1 = array[array_ix].sbit_offset() as usize;
                    let offset2 = array
                        .get(array_ix + 1)
                        .ok_or(ReadError::OutOfBounds)?
                        .sbit_offset() as usize;
                    location.data_offset = offset1;
                    location.data_size = Some(offset2 - offset1);
                }
                IndexSubtable::Format5(st) => {
                    location.format = st.image_format();
                    let array = st.glyph_array();
                    if array.binary_search_by(|x| x.get().cmp(&glyph_id)).is_err() {
                        return Err(ReadError::InvalidCollectionIndex(glyph_id.to_u16() as u32));
                    }
                    let data_size = st.image_size() as usize;
                    location.data_size = Some(data_size);
                    location.data_offset = st.image_data_offset() as usize + glyph_ix * data_size;
                    location.metrics = Some(st.big_metrics()[0].clone());
                }
            }
            return Ok(location);
        }
        Err(ReadError::OutOfBounds)
    }

    fn subtable<'a>(
        &self,
        offset_data: FontData<'a>,
        index: u32,
    ) -> Result<BitmapSizeSubtable<'a>, ReadError> {
        let base_offset = self.index_subtable_array_offset() as usize;
        const SUBTABLE_HEADER_SIZE: usize = 8;
        let header_offset = base_offset + index as usize * SUBTABLE_HEADER_SIZE;
        let header_data = offset_data
            .slice(header_offset..)
            .ok_or(ReadError::OutOfBounds)?;
        let header = IndexSubtableArray::read(header_data)?;
        let subtable_offset = base_offset + header.additional_offset_to_index_subtable() as usize;
        let subtable_data = offset_data
            .slice(subtable_offset..)
            .ok_or(ReadError::OutOfBounds)?;
        let subtable = IndexSubtable::read(subtable_data)?;
        Ok(BitmapSizeSubtable {
            first_glyph_index: header.first_glyph_index(),
            last_glyph_index: header.last_glyph_index(),
            kind: subtable,
        })
    }
}

struct BitmapSizeSubtable<'a> {
    pub first_glyph_index: GlyphId,
    pub last_glyph_index: GlyphId,
    pub kind: IndexSubtable<'a>,
}

#[derive(Clone, Default)]
pub struct BitmapLocation {
    /// Format of EBDT/CBDT image data.
    pub format: u16,
    /// Offset in bytes from the start of the EBDT/CBDT table.
    pub data_offset: usize,
    /// Size of the image data in bytes, if present in the EBLC/CBLC table.
    pub data_size: Option<usize>,
    /// Bit depth from the associated size. Required for computing image data
    /// size when unspecified.
    pub bit_depth: u8,
    /// Full metrics, if present in the EBLC/CBLC table.
    pub metrics: Option<BigGlyphMetrics>,
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum BitmapDataFormat {
    /// The full bitmap is tightly packed according to the bit depth.
    BitAligned,
    /// Each row of the data is aligned to a byte boundary.
    ByteAligned,
    Png,
}

#[derive(Clone)]
pub enum BitmapMetrics {
    Small(SmallGlyphMetrics),
    Big(BigGlyphMetrics),
}

#[derive(Clone)]
pub struct BitmapData<'a> {
    pub metrics: BitmapMetrics,
    pub content: BitmapContent<'a>,
}

#[derive(Clone)]
pub enum BitmapContent<'a> {
    Data(BitmapDataFormat, &'a [u8]),
    Composite(&'a [BdtComponent]),
}

pub(crate) fn bitmap_data<'a>(
    offset_data: FontData<'a>,
    location: &BitmapLocation,
    is_color: bool,
) -> Result<BitmapData<'a>, ReadError> {
    let mut image_data = offset_data
        .slice(location.data_offset..)
        .ok_or(ReadError::OutOfBounds)?
        .cursor();
    match location.format {
        // Small metrics, byte-aligned data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-1-small-metrics-byte-aligned-data>
        1 => {
            let metrics = read_small_metrics(&mut image_data)?;
            // The data for each row is padded to a byte boundary
            let pitch = (metrics.width as usize * location.bit_depth as usize + 7) / 8;
            let height = metrics.height as usize;
            let data = image_data.read_array::<u8>(pitch * height)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Small(metrics),
                content: BitmapContent::Data(BitmapDataFormat::ByteAligned, data),
            })
        }
        // Small metrics, bit-aligned data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-2-small-metrics-bit-aligned-data>
        2 => {
            let metrics = read_small_metrics(&mut image_data)?;
            let width = metrics.width as usize * location.bit_depth as usize;
            let height = metrics.height as usize;
            // The data is tightly packed
            let data = image_data.read_array::<u8>((width * height + 7) / 8)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Small(metrics),
                content: BitmapContent::Data(BitmapDataFormat::BitAligned, data),
            })
        }
        // Format 3 is obsolete
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-3-obsolete>
        // Format 4 is not supported
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-4-not-supported-metrics-in-eblc-compressed-data>
        // ---
        // Metrics in EBLC/CBLC, bit-aligned image data only
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-5-metrics-in-eblc-bit-aligned-image-data-only>
        5 => {
            let metrics = location.metrics.clone().ok_or(ReadError::MalformedData(
                "expected metrics from location table",
            ))?;
            let width = metrics.width as usize * location.bit_depth as usize;
            let height = metrics.height as usize;
            // The data is tightly packed
            let data = image_data.read_array::<u8>((width * height + 7) / 8)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Data(BitmapDataFormat::BitAligned, data),
            })
        }
        // Big metrics, byte-aligned data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-6-big-metrics-byte-aligned-data>
        6 => {
            let metrics = read_big_metrics(&mut image_data)?;
            // The data for each row is padded to a byte boundary
            let pitch = (metrics.width as usize * location.bit_depth as usize + 7) / 8;
            let height = metrics.height as usize;
            let data = image_data.read_array::<u8>(pitch * height)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Data(BitmapDataFormat::ByteAligned, data),
            })
        }
        // Big metrics, bit-aligned data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format7-big-metrics-bit-aligned-data>
        7 => {
            let metrics = read_big_metrics(&mut image_data)?;
            let width = metrics.width as usize * location.bit_depth as usize;
            let height = metrics.height as usize;
            // The data is tightly packed
            let data = image_data.read_array::<u8>((width * height + 7) / 8)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Data(BitmapDataFormat::BitAligned, data),
            })
        }
        // Small metrics, component data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-8-small-metrics-component-data>
        8 => {
            let metrics = read_small_metrics(&mut image_data)?;
            let _pad = image_data.read::<u8>()?;
            let count = image_data.read::<u16>()? as usize;
            let components = image_data.read_array::<BdtComponent>(count)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Small(metrics),
                content: BitmapContent::Composite(components),
            })
        }
        // Big metrics, component data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt#format-9-big-metrics-component-data>
        9 => {
            let metrics = read_big_metrics(&mut image_data)?;
            let count = image_data.read::<u16>()? as usize;
            let components = image_data.read_array::<BdtComponent>(count)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Composite(components),
            })
        }
        // Small metrics, PNG image data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/cbdt#format-17-small-metrics-png-image-data>
        17 if is_color => {
            let metrics = read_small_metrics(&mut image_data)?;
            let data_len = image_data.read::<u32>()? as usize;
            let data = image_data.read_array::<u8>(data_len)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Small(metrics),
                content: BitmapContent::Data(BitmapDataFormat::Png, data),
            })
        }
        // Big metrics, PNG image data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/cbdt#format-18-big-metrics-png-image-data>
        18 if is_color => {
            let metrics = read_big_metrics(&mut image_data)?;
            let data_len = image_data.read::<u32>()? as usize;
            let data = image_data.read_array::<u8>(data_len)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Data(BitmapDataFormat::Png, data),
            })
        }
        // Metrics in CBLC table, PNG image data
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/cbdt#format-19-metrics-in-cblc-table-png-image-data>
        19 if is_color => {
            let metrics = location.metrics.clone().ok_or(ReadError::MalformedData(
                "expected metrics from location table",
            ))?;
            let data_len = image_data.read::<u32>()? as usize;
            let data = image_data.read_array::<u8>(data_len)?;
            Ok(BitmapData {
                metrics: BitmapMetrics::Big(metrics),
                content: BitmapContent::Data(BitmapDataFormat::Png, data),
            })
        }
        _ => Err(ReadError::MalformedData("unexpected bitmap data format")),
    }
}

fn read_small_metrics(cursor: &mut Cursor) -> Result<SmallGlyphMetrics, ReadError> {
    Ok(cursor.read_array::<SmallGlyphMetrics>(1)?[0].clone())
}

fn read_big_metrics(cursor: &mut Cursor) -> Result<BigGlyphMetrics, ReadError> {
    Ok(cursor.read_array::<BigGlyphMetrics>(1)?[0].clone())
}

#[cfg(feature = "traversal")]
impl SbitLineMetrics {
    pub(crate) fn traversal_type<'a>(&self, data: FontData<'a>) -> FieldType<'a> {
        FieldType::Record(self.traverse(data))
    }
}
