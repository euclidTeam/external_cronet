// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: google/protobuf/empty.proto

#include <google/protobuf/empty.pb.h>

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

PROTOBUF_NAMESPACE_OPEN
PROTOBUF_CONSTEXPR Empty::Empty(
    ::_pbi::ConstantInitialized) {}
struct EmptyDefaultTypeInternal {
  PROTOBUF_CONSTEXPR EmptyDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~EmptyDefaultTypeInternal() {}
  union {
    Empty _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 EmptyDefaultTypeInternal _Empty_default_instance_;
PROTOBUF_NAMESPACE_CLOSE
static ::_pb::Metadata file_level_metadata_google_2fprotobuf_2fempty_2eproto[1];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_google_2fprotobuf_2fempty_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_google_2fprotobuf_2fempty_2eproto = nullptr;

const uint32_t TableStruct_google_2fprotobuf_2fempty_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::PROTOBUF_NAMESPACE_ID::Empty, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::PROTOBUF_NAMESPACE_ID::Empty)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::PROTOBUF_NAMESPACE_ID::_Empty_default_instance_._instance,
};

const char descriptor_table_protodef_google_2fprotobuf_2fempty_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\033google/protobuf/empty.proto\022\017google.pr"
  "otobuf\"\007\n\005EmptyB}\n\023com.google.protobufB\n"
  "EmptyProtoP\001Z.google.golang.org/protobuf"
  "/types/known/emptypb\370\001\001\242\002\003GPB\252\002\036Google.P"
  "rotobuf.WellKnownTypesb\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_google_2fprotobuf_2fempty_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_google_2fprotobuf_2fempty_2eproto = {
    false, false, 190, descriptor_table_protodef_google_2fprotobuf_2fempty_2eproto,
    "google/protobuf/empty.proto",
    &descriptor_table_google_2fprotobuf_2fempty_2eproto_once, nullptr, 0, 1,
    schemas, file_default_instances, TableStruct_google_2fprotobuf_2fempty_2eproto::offsets,
    file_level_metadata_google_2fprotobuf_2fempty_2eproto, file_level_enum_descriptors_google_2fprotobuf_2fempty_2eproto,
    file_level_service_descriptors_google_2fprotobuf_2fempty_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_google_2fprotobuf_2fempty_2eproto_getter() {
  return &descriptor_table_google_2fprotobuf_2fempty_2eproto;
}

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_google_2fprotobuf_2fempty_2eproto(&descriptor_table_google_2fprotobuf_2fempty_2eproto);
PROTOBUF_NAMESPACE_OPEN

// ===================================================================

class Empty::_Internal {
 public:
};

Empty::Empty(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::internal::ZeroFieldsBase(arena, is_message_owned) {
  // @@protoc_insertion_point(arena_constructor:google.protobuf.Empty)
}
Empty::Empty(const Empty& from)
  : ::PROTOBUF_NAMESPACE_ID::internal::ZeroFieldsBase() {
  Empty* const _this = this; (void)_this;
  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  // @@protoc_insertion_point(copy_constructor:google.protobuf.Empty)
}





const ::PROTOBUF_NAMESPACE_ID::Message::ClassData Empty::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::internal::ZeroFieldsBase::CopyImpl,
    ::PROTOBUF_NAMESPACE_ID::internal::ZeroFieldsBase::MergeImpl,
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*Empty::GetClassData() const { return &_class_data_; }







::PROTOBUF_NAMESPACE_ID::Metadata Empty::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_google_2fprotobuf_2fempty_2eproto_getter, &descriptor_table_google_2fprotobuf_2fempty_2eproto_once,
      file_level_metadata_google_2fprotobuf_2fempty_2eproto[0]);
}

// @@protoc_insertion_point(namespace_scope)
PROTOBUF_NAMESPACE_CLOSE
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::PROTOBUF_NAMESPACE_ID::Empty*
Arena::CreateMaybeMessage< ::PROTOBUF_NAMESPACE_ID::Empty >(Arena* arena) {
  return Arena::CreateMessageInternal< ::PROTOBUF_NAMESPACE_ID::Empty >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
