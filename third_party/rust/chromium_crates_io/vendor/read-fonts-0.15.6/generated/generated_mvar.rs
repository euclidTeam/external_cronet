// THIS FILE IS AUTOGENERATED.
// Any changes to this file will be overwritten.
// For more information about how codegen works, see font-codegen/README.md

#[allow(unused_imports)]
use crate::codegen_prelude::*;

/// The [MVAR (Metrics Variations)](https://docs.microsoft.com/en-us/typography/opentype/spec/mvar) table
#[derive(Debug, Clone, Copy)]
#[doc(hidden)]
pub struct MvarMarker {
    value_records_byte_len: usize,
}

impl MvarMarker {
    fn version_byte_range(&self) -> Range<usize> {
        let start = 0;
        start..start + MajorMinor::RAW_BYTE_LEN
    }
    fn _reserved_byte_range(&self) -> Range<usize> {
        let start = self.version_byte_range().end;
        start..start + u16::RAW_BYTE_LEN
    }
    fn value_record_size_byte_range(&self) -> Range<usize> {
        let start = self._reserved_byte_range().end;
        start..start + u16::RAW_BYTE_LEN
    }
    fn value_record_count_byte_range(&self) -> Range<usize> {
        let start = self.value_record_size_byte_range().end;
        start..start + u16::RAW_BYTE_LEN
    }
    fn item_variation_store_offset_byte_range(&self) -> Range<usize> {
        let start = self.value_record_count_byte_range().end;
        start..start + Offset16::RAW_BYTE_LEN
    }
    fn value_records_byte_range(&self) -> Range<usize> {
        let start = self.item_variation_store_offset_byte_range().end;
        start..start + self.value_records_byte_len
    }
}

impl TopLevelTable for Mvar<'_> {
    /// `MVAR`
    const TAG: Tag = Tag::new(b"MVAR");
}

impl<'a> FontRead<'a> for Mvar<'a> {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        let mut cursor = data.cursor();
        cursor.advance::<MajorMinor>();
        cursor.advance::<u16>();
        cursor.advance::<u16>();
        let value_record_count: u16 = cursor.read()?;
        cursor.advance::<Offset16>();
        let value_records_byte_len = value_record_count as usize * ValueRecord::RAW_BYTE_LEN;
        cursor.advance_by(value_records_byte_len);
        cursor.finish(MvarMarker {
            value_records_byte_len,
        })
    }
}

/// The [MVAR (Metrics Variations)](https://docs.microsoft.com/en-us/typography/opentype/spec/mvar) table
pub type Mvar<'a> = TableRef<'a, MvarMarker>;

impl<'a> Mvar<'a> {
    /// Major version number of the horizontal metrics variations table — set to 1.
    /// Minor version number of the horizontal metrics variations table — set to 0.
    pub fn version(&self) -> MajorMinor {
        let range = self.shape.version_byte_range();
        self.data.read_at(range.start).unwrap()
    }

    /// The size in bytes of each value record — must be greater than zero.
    pub fn value_record_size(&self) -> u16 {
        let range = self.shape.value_record_size_byte_range();
        self.data.read_at(range.start).unwrap()
    }

    /// The number of value records — may be zero.
    pub fn value_record_count(&self) -> u16 {
        let range = self.shape.value_record_count_byte_range();
        self.data.read_at(range.start).unwrap()
    }

    /// Offset in bytes from the start of this table to the item variation store table. If valueRecordCount is zero, set to zero; if valueRecordCount is greater than zero, must be greater than zero.
    pub fn item_variation_store_offset(&self) -> Nullable<Offset16> {
        let range = self.shape.item_variation_store_offset_byte_range();
        self.data.read_at(range.start).unwrap()
    }

    /// Attempt to resolve [`item_variation_store_offset`][Self::item_variation_store_offset].
    pub fn item_variation_store(&self) -> Option<Result<ItemVariationStore<'a>, ReadError>> {
        let data = self.data;
        self.item_variation_store_offset().resolve(data)
    }

    /// Array of value records that identify target items and the associated delta-set index for each. The valueTag records must be in binary order of their valueTag field.
    pub fn value_records(&self) -> &'a [ValueRecord] {
        let range = self.shape.value_records_byte_range();
        self.data.read_array(range).unwrap()
    }
}

#[cfg(feature = "traversal")]
impl<'a> SomeTable<'a> for Mvar<'a> {
    fn type_name(&self) -> &str {
        "Mvar"
    }
    fn get_field(&self, idx: usize) -> Option<Field<'a>> {
        match idx {
            0usize => Some(Field::new("version", self.version())),
            1usize => Some(Field::new("value_record_size", self.value_record_size())),
            2usize => Some(Field::new("value_record_count", self.value_record_count())),
            3usize => Some(Field::new(
                "item_variation_store_offset",
                FieldType::offset(
                    self.item_variation_store_offset(),
                    self.item_variation_store(),
                ),
            )),
            4usize => Some(Field::new(
                "value_records",
                traversal::FieldType::array_of_records(
                    stringify!(ValueRecord),
                    self.value_records(),
                    self.offset_data(),
                ),
            )),
            _ => None,
        }
    }
}

#[cfg(feature = "traversal")]
impl<'a> std::fmt::Debug for Mvar<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        (self as &dyn SomeTable<'a>).fmt(f)
    }
}

/// [ValueRecord](https://learn.microsoft.com/en-us/typography/opentype/spec/mvar#table-formats) metrics variation record
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(C)]
#[repr(packed)]
pub struct ValueRecord {
    /// Four-byte tag identifying a font-wide measure.
    pub value_tag: BigEndian<Tag>,
    /// A delta-set outer index — used to select an item variation data subtable within the item variation store.
    pub delta_set_outer_index: BigEndian<u16>,
    /// A delta-set inner index — used to select a delta-set row within an item variation data subtable.
    pub delta_set_inner_index: BigEndian<u16>,
}

impl ValueRecord {
    /// Four-byte tag identifying a font-wide measure.
    pub fn value_tag(&self) -> Tag {
        self.value_tag.get()
    }

    /// A delta-set outer index — used to select an item variation data subtable within the item variation store.
    pub fn delta_set_outer_index(&self) -> u16 {
        self.delta_set_outer_index.get()
    }

    /// A delta-set inner index — used to select a delta-set row within an item variation data subtable.
    pub fn delta_set_inner_index(&self) -> u16 {
        self.delta_set_inner_index.get()
    }
}

impl FixedSize for ValueRecord {
    const RAW_BYTE_LEN: usize = Tag::RAW_BYTE_LEN + u16::RAW_BYTE_LEN + u16::RAW_BYTE_LEN;
}

impl sealed::Sealed for ValueRecord {}

/// SAFETY: see the [`FromBytes`] trait documentation.
unsafe impl FromBytes for ValueRecord {
    fn this_trait_should_only_be_implemented_in_generated_code() {}
}

#[cfg(feature = "traversal")]
impl<'a> SomeRecord<'a> for ValueRecord {
    fn traverse(self, data: FontData<'a>) -> RecordResolver<'a> {
        RecordResolver {
            name: "ValueRecord",
            get_field: Box::new(move |idx, _data| match idx {
                0usize => Some(Field::new("value_tag", self.value_tag())),
                1usize => Some(Field::new(
                    "delta_set_outer_index",
                    self.delta_set_outer_index(),
                )),
                2usize => Some(Field::new(
                    "delta_set_inner_index",
                    self.delta_set_inner_index(),
                )),
                _ => None,
            }),
            data,
        }
    }
}
