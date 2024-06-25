// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms
// Licensed under the MIT License <LICENSE.md>
//! Mappings for the contents of ks.h
DEFINE_GUID!{KSCATEGORY_BRIDGE,
    0x085AFF00, 0x62CE, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_CAPTURE,
    0x65E8773D, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_VIDEO_CAMERA,
    0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44}
DEFINE_GUID!{KSCATEGORY_SENSOR_CAMERA,
    0x24e552d7, 0x6523, 0x47f7, 0xa6, 0x47, 0xd3, 0x46, 0x5b, 0xf1, 0xf5, 0xca}
DEFINE_GUID!{KSCATEGORY_SENSOR_GROUP,
    0x669C7214, 0x0A88, 0x4311, 0xA7, 0xF3, 0x4E, 0x79, 0x82, 0x0E, 0x33, 0xBD}
DEFINE_GUID!{KSCATEGORY_RENDER,
    0x65E8773E, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_MIXER,
    0xAD809C00, 0x7B88, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_SPLITTER,
    0x0A4252A0, 0x7E70, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_DATACOMPRESSOR,
    0x1E84C900, 0x7E70, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_DATADECOMPRESSOR,
    0x2721AE20, 0x7E70, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_DATATRANSFORM,
    0x2EB07EA0, 0x7E70, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSMFT_CATEGORY_VIDEO_DECODER,
    0xd6c02d4b, 0x6833, 0x45b4, 0x97, 0x1a, 0x05, 0xa4, 0xb0, 0x4b, 0xab, 0x91}
DEFINE_GUID!{KSMFT_CATEGORY_VIDEO_ENCODER,
    0xf79eac7d, 0xe545, 0x4387, 0xbd, 0xee, 0xd6, 0x47, 0xd7, 0xbd, 0xe4, 0x2a}
DEFINE_GUID!{KSMFT_CATEGORY_VIDEO_EFFECT,
    0x12e17c21, 0x532c, 0x4a6e, 0x8a, 0x1c, 0x40, 0x82, 0x5a, 0x73, 0x63, 0x97}
DEFINE_GUID!{KSMFT_CATEGORY_MULTIPLEXER,
    0x059c561e, 0x05ae, 0x4b61, 0xb6, 0x9d, 0x55, 0xb6, 0x1e, 0xe5, 0x4a, 0x7b}
DEFINE_GUID!{KSMFT_CATEGORY_DEMULTIPLEXER,
    0xa8700a7a, 0x939b, 0x44c5, 0x99, 0xd7, 0x76, 0x22, 0x6b, 0x23, 0xb3, 0xf1}
DEFINE_GUID!{KSMFT_CATEGORY_AUDIO_DECODER,
    0x9ea73fb4, 0xef7a, 0x4559, 0x8d, 0x5d, 0x71, 0x9d, 0x8f, 0x04, 0x26, 0xc7}
DEFINE_GUID!{KSMFT_CATEGORY_AUDIO_ENCODER,
    0x91c64bd0, 0xf91e, 0x4d8c, 0x92, 0x76, 0xdb, 0x24, 0x82, 0x79, 0xd9, 0x75}
DEFINE_GUID!{KSMFT_CATEGORY_AUDIO_EFFECT,
    0x11064c48, 0x3648, 0x4ed0, 0x93, 0x2e, 0x05, 0xce, 0x8a, 0xc8, 0x11, 0xb7}
DEFINE_GUID!{KSMFT_CATEGORY_VIDEO_PROCESSOR,
    0x302ea3fc, 0xaa5f, 0x47f9, 0x9f, 0x7a, 0xc2, 0x18, 0x8b, 0xb1, 0x63, 0x02}
DEFINE_GUID!{KSMFT_CATEGORY_OTHER,
    0x90175d57, 0xb7ea, 0x4901, 0xae, 0xb3, 0x93, 0x3a, 0x87, 0x47, 0x75, 0x6f}
DEFINE_GUID!{KSCATEGORY_COMMUNICATIONSTRANSFORM,
    0xCF1DDA2C, 0x9743, 0x11D0, 0xA3, 0xEE, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_INTERFACETRANSFORM,
    0xCF1DDA2D, 0x9743, 0x11D0, 0xA3, 0xEE, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_MEDIUMTRANSFORM,
    0xCF1DDA2E, 0x9743, 0x11D0, 0xA3, 0xEE, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_FILESYSTEM,
    0x760FED5E, 0x9357, 0x11D0, 0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_CLOCK,
    0x53172480, 0x4791, 0x11D0, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}
DEFINE_GUID!{KSCATEGORY_PROXY,
    0x97EBAACA, 0x95BD, 0x11D0, 0xA3, 0xEA, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
DEFINE_GUID!{KSCATEGORY_QUALITY,
    0x97EBAACB, 0x95BD, 0x11D0, 0xA3, 0xEA, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}
