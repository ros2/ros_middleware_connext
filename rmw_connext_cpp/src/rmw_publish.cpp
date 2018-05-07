// Copyright 2014-2017 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rmw/error_handling.h"
#include "rmw/rmw.h"
#include "rmw/types.h"

#include "rmw_connext_cpp/connext_static_publisher_info.hpp"
#include "rmw_connext_cpp/identifier.hpp"

#include "rosidl_typesupport_connext_cpp/connext_static_cdr_stream.hpp"

// include patched generated code from the build folder
#include "connext_static_raw_dataSupport.h"

bool
publish(DDSDataWriter * dds_data_writer, ConnextStaticCDRStream * cdr_stream)
{
  ConnextStaticRawDataDataWriter * data_writer =
    ConnextStaticRawDataDataWriter::narrow(dds_data_writer);
  if (!data_writer) {
    RMW_SET_ERROR_MSG("failed to narrow data writer");
    return false;
  }

  ConnextStaticRawData * instance = ConnextStaticRawDataTypeSupport::create_data();
  if (!instance) {
    RMW_SET_ERROR_MSG("failed to create dds message instance");
    return false;
  }

  DDS_ReturnCode_t status = DDS_RETCODE_ERROR;

  instance->serialized_data.maximum(0);
  if (!instance->serialized_data.loan_contiguous(
      reinterpret_cast<DDS_Octet *>(cdr_stream->buffer),
      cdr_stream->buffer_length, cdr_stream->buffer_length))
  {
    RMW_SET_ERROR_MSG("failed to loan memory for message");
    goto cleanup;
  }

  status = data_writer->write(*instance, DDS_HANDLE_NIL);

cleanup:
  if (instance) {
    if (!instance->serialized_data.unloan()) {
      fprintf(stderr, "failed to return loaned memory\n");
      status = DDS_RETCODE_ERROR;
    }
    ConnextStaticRawDataTypeSupport::delete_data(instance);
  }

  return status == DDS_RETCODE_OK;
}

extern "C"
{
rmw_ret_t
rmw_publish(const rmw_publisher_t * publisher, const void * ros_message)
{
  if (!publisher) {
    RMW_SET_ERROR_MSG("publisher handle is null");
    return RMW_RET_ERROR;
  }
  if (publisher->implementation_identifier != rti_connext_identifier) {
    RMW_SET_ERROR_MSG("publisher handle is not from this rmw implementation");
    return RMW_RET_ERROR;
  }
  if (!ros_message) {
    RMW_SET_ERROR_MSG("ros message handle is null");
    return RMW_RET_ERROR;
  }

  ConnextStaticPublisherInfo * publisher_info =
    static_cast<ConnextStaticPublisherInfo *>(publisher->data);
  if (!publisher_info) {
    RMW_SET_ERROR_MSG("publisher info handle is null");
    return RMW_RET_ERROR;
  }
  const message_type_support_callbacks_t * callbacks = publisher_info->callbacks_;
  if (!callbacks) {
    RMW_SET_ERROR_MSG("callbacks handle is null");
    return RMW_RET_ERROR;
  }
  DDSDataWriter * topic_writer = publisher_info->topic_writer_;
  if (!topic_writer) {
    RMW_SET_ERROR_MSG("topic writer handle is null");
    return RMW_RET_ERROR;
  }

  auto ret = RMW_RET_OK;
  ConnextStaticCDRStream cdr_stream;
  cdr_stream.allocator = rcutils_get_default_allocator();
  if (!callbacks->to_cdr_stream(ros_message, &cdr_stream)) {
    RMW_SET_ERROR_MSG("failed to convert ros_message to cdr stream");
    ret = RMW_RET_ERROR;
    goto fail;
  }
  if (cdr_stream.buffer_length == 0) {
    RMW_SET_ERROR_MSG("no message length set");
    ret = RMW_RET_ERROR;
    goto fail;
  }
  if (!cdr_stream.buffer) {
    RMW_SET_ERROR_MSG("no raw message attached");
    ret = RMW_RET_ERROR;
    goto fail;
  }
  if (!publish(topic_writer, &cdr_stream)) {
    RMW_SET_ERROR_MSG("failed to publish message");
    ret = RMW_RET_ERROR;
    goto fail;
  }

fail:
  cdr_stream.allocator.deallocate(cdr_stream.buffer, cdr_stream.allocator.state);
  return ret;
}

rmw_ret_t
rmw_publish_raw(const rmw_publisher_t * publisher, const rmw_message_raw_t * raw_message)
{
  if (!publisher) {
    RMW_SET_ERROR_MSG("publisher handle is null");
    return RMW_RET_ERROR;
  }
  if (publisher->implementation_identifier != rti_connext_identifier) {
    RMW_SET_ERROR_MSG("publisher handle is not from this rmw implementation");
    return RMW_RET_ERROR;
  }
  if (!raw_message) {
    RMW_SET_ERROR_MSG("raw message handle is null");
    return RMW_RET_ERROR;
  }

  ConnextStaticPublisherInfo * publisher_info =
    static_cast<ConnextStaticPublisherInfo *>(publisher->data);
  if (!publisher_info) {
    RMW_SET_ERROR_MSG("publisher info handle is null");
    return RMW_RET_ERROR;
  }
  const message_type_support_callbacks_t * callbacks = publisher_info->callbacks_;
  if (!callbacks) {
    RMW_SET_ERROR_MSG("callbacks handle is null");
    return RMW_RET_ERROR;
  }
  DDSDataWriter * topic_writer = publisher_info->topic_writer_;
  if (!topic_writer) {
    RMW_SET_ERROR_MSG("topic writer handle is null");
    return RMW_RET_ERROR;
  }

  ConnextStaticCDRStream cdr_stream;
  cdr_stream.buffer = raw_message->buffer;
  cdr_stream.buffer_length = raw_message->buffer_length;
  cdr_stream.buffer_capacity = raw_message->buffer_capacity;
  bool published = publish(topic_writer, &cdr_stream);
  if (!published) {
    RMW_SET_ERROR_MSG("failed to publish message");
    return RMW_RET_ERROR;
  }
  return RMW_RET_OK;
}
}  // extern "C"
