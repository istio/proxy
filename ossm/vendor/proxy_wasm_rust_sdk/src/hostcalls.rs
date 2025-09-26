// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use crate::dispatcher;
use crate::types::*;
use std::ptr::{null, null_mut};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

extern "C" {
    fn proxy_log(level: LogLevel, message_data: *const u8, message_size: usize) -> Status;
}

pub fn log(level: LogLevel, message: &str) -> Result<(), Status> {
    unsafe {
        match proxy_log(level, message.as_ptr(), message.len()) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_log_level(return_level: *mut LogLevel) -> Status;
}

pub fn get_log_level() -> Result<LogLevel, Status> {
    let mut return_level: LogLevel = LogLevel::Trace;
    unsafe {
        match proxy_get_log_level(&mut return_level) {
            Status::Ok => Ok(return_level),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_current_time_nanoseconds(return_time: *mut u64) -> Status;
}

pub fn get_current_time() -> Result<SystemTime, Status> {
    let mut return_time: u64 = 0;
    unsafe {
        match proxy_get_current_time_nanoseconds(&mut return_time) {
            Status::Ok => Ok(UNIX_EPOCH + Duration::from_nanos(return_time)),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_tick_period_milliseconds(period: u32) -> Status;
}

pub fn set_tick_period(period: Duration) -> Result<(), Status> {
    unsafe {
        match proxy_set_tick_period_milliseconds(period.as_millis() as u32) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_buffer_bytes(
        buffer_type: BufferType,
        start: usize,
        max_size: usize,
        return_buffer_data: *mut *mut u8,
        return_buffer_size: *mut usize,
    ) -> Status;
}

pub fn get_buffer(
    buffer_type: BufferType,
    start: usize,
    max_size: usize,
) -> Result<Option<Bytes>, Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_get_buffer_bytes(
            buffer_type,
            start,
            max_size,
            &mut return_data,
            &mut return_size,
        ) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(Vec::from_raw_parts(
                        return_data,
                        return_size,
                        return_size,
                    )))
                } else {
                    Ok(None)
                }
            }
            Status::NotFound => Ok(None),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_buffer_bytes(
        buffer_type: BufferType,
        start: usize,
        size: usize,
        buffer_data: *const u8,
        buffer_size: usize,
    ) -> Status;
}

pub fn set_buffer(
    buffer_type: BufferType,
    start: usize,
    size: usize,
    value: &[u8],
) -> Result<(), Status> {
    unsafe {
        match proxy_set_buffer_bytes(buffer_type, start, size, value.as_ptr(), value.len()) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_header_map_pairs(
        map_type: MapType,
        return_map_data: *mut *mut u8,
        return_map_size: *mut usize,
    ) -> Status;
}

pub fn get_map(map_type: MapType) -> Result<Vec<(String, String)>, Status> {
    unsafe {
        let mut return_data: *mut u8 = null_mut();
        let mut return_size: usize = 0;
        match proxy_get_header_map_pairs(map_type, &mut return_data, &mut return_size) {
            Status::Ok => {
                if !return_data.is_null() {
                    let serialized_map = Vec::from_raw_parts(return_data, return_size, return_size);
                    Ok(utils::deserialize_map(&serialized_map))
                } else {
                    Ok(Vec::new())
                }
            }
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn get_map_bytes(map_type: MapType) -> Result<Vec<(String, Bytes)>, Status> {
    unsafe {
        let mut return_data: *mut u8 = null_mut();
        let mut return_size: usize = 0;
        match proxy_get_header_map_pairs(map_type, &mut return_data, &mut return_size) {
            Status::Ok => {
                if !return_data.is_null() {
                    let serialized_map = Vec::from_raw_parts(return_data, return_size, return_size);
                    Ok(utils::deserialize_map_bytes(&serialized_map))
                } else {
                    Ok(Vec::new())
                }
            }
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_header_map_pairs(
        map_type: MapType,
        map_data: *const u8,
        map_size: usize,
    ) -> Status;
}

pub fn set_map(map_type: MapType, map: Vec<(&str, &str)>) -> Result<(), Status> {
    let serialized_map = utils::serialize_map(map);
    unsafe {
        match proxy_set_header_map_pairs(map_type, serialized_map.as_ptr(), serialized_map.len()) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn set_map_bytes(map_type: MapType, map: Vec<(&str, &[u8])>) -> Result<(), Status> {
    let serialized_map = utils::serialize_map_bytes(map);
    unsafe {
        match proxy_set_header_map_pairs(map_type, serialized_map.as_ptr(), serialized_map.len()) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
        return_value_data: *mut *mut u8,
        return_value_size: *mut usize,
    ) -> Status;
}

pub fn get_map_value(map_type: MapType, key: &str) -> Result<Option<String>, Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_get_header_map_value(
            map_type,
            key.as_ptr(),
            key.len(),
            &mut return_data,
            &mut return_size,
        ) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(
                        String::from_utf8(Vec::from_raw_parts(
                            return_data,
                            return_size,
                            return_size,
                        ))
                        .unwrap(),
                    ))
                } else {
                    Ok(None)
                }
            }
            Status::NotFound => Ok(None),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn get_map_value_bytes(map_type: MapType, key: &str) -> Result<Option<Bytes>, Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_get_header_map_value(
            map_type,
            key.as_ptr(),
            key.len(),
            &mut return_data,
            &mut return_size,
        ) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(Vec::from_raw_parts(
                        return_data,
                        return_size,
                        return_size,
                    )))
                } else {
                    Ok(None)
                }
            }
            Status::NotFound => Ok(None),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_replace_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

extern "C" {
    fn proxy_remove_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
    ) -> Status;
}

pub fn set_map_value(map_type: MapType, key: &str, value: Option<&str>) -> Result<(), Status> {
    unsafe {
        if let Some(value) = value {
            match proxy_replace_header_map_value(
                map_type,
                key.as_ptr(),
                key.len(),
                value.as_ptr(),
                value.len(),
            ) {
                Status::Ok => Ok(()),
                status => panic!("unexpected status: {}", status as u32),
            }
        } else {
            match proxy_remove_header_map_value(map_type, key.as_ptr(), key.len()) {
                Status::Ok => Ok(()),
                status => panic!("unexpected status: {}", status as u32),
            }
        }
    }
}

pub fn set_map_value_bytes(
    map_type: MapType,
    key: &str,
    value: Option<&[u8]>,
) -> Result<(), Status> {
    unsafe {
        if let Some(value) = value {
            match proxy_replace_header_map_value(
                map_type,
                key.as_ptr(),
                key.len(),
                value.as_ptr(),
                value.len(),
            ) {
                Status::Ok => Ok(()),
                status => panic!("unexpected status: {}", status as u32),
            }
        } else {
            match proxy_remove_header_map_value(map_type, key.as_ptr(), key.len()) {
                Status::Ok => Ok(()),
                status => panic!("unexpected status: {}", status as u32),
            }
        }
    }
}

extern "C" {
    fn proxy_add_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

pub fn add_map_value(map_type: MapType, key: &str, value: &str) -> Result<(), Status> {
    unsafe {
        match proxy_add_header_map_value(
            map_type,
            key.as_ptr(),
            key.len(),
            value.as_ptr(),
            value.len(),
        ) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn add_map_value_bytes(map_type: MapType, key: &str, value: &[u8]) -> Result<(), Status> {
    unsafe {
        match proxy_add_header_map_value(
            map_type,
            key.as_ptr(),
            key.len(),
            value.as_ptr(),
            value.len(),
        ) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_property(
        path_data: *const u8,
        path_size: usize,
        return_value_data: *mut *mut u8,
        return_value_size: *mut usize,
    ) -> Status;
}

pub fn get_property(path: Vec<&str>) -> Result<Option<Bytes>, Status> {
    let serialized_path = utils::serialize_property_path(path);
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_get_property(
            serialized_path.as_ptr(),
            serialized_path.len(),
            &mut return_data,
            &mut return_size,
        ) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(Vec::from_raw_parts(
                        return_data,
                        return_size,
                        return_size,
                    )))
                } else {
                    Ok(None)
                }
            }
            Status::NotFound => Ok(None),
            Status::SerializationFailure => Err(Status::SerializationFailure),
            Status::InternalFailure => Err(Status::InternalFailure),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_property(
        path_data: *const u8,
        path_size: usize,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

pub fn set_property(path: Vec<&str>, value: Option<&[u8]>) -> Result<(), Status> {
    let serialized_path = utils::serialize_property_path(path);
    unsafe {
        match proxy_set_property(
            serialized_path.as_ptr(),
            serialized_path.len(),
            value.map_or(null(), |value| value.as_ptr()),
            value.map_or(0, |value| value.len()),
        ) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_shared_data(
        key_data: *const u8,
        key_size: usize,
        return_value_data: *mut *mut u8,
        return_value_size: *mut usize,
        return_cas: *mut u32,
    ) -> Status;
}

pub fn get_shared_data(key: &str) -> Result<(Option<Bytes>, Option<u32>), Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    let mut return_cas: u32 = 0;
    unsafe {
        match proxy_get_shared_data(
            key.as_ptr(),
            key.len(),
            &mut return_data,
            &mut return_size,
            &mut return_cas,
        ) {
            Status::Ok => {
                let cas = match return_cas {
                    0 => None,
                    cas => Some(cas),
                };
                if !return_data.is_null() {
                    Ok((
                        Some(Vec::from_raw_parts(return_data, return_size, return_size)),
                        cas,
                    ))
                } else {
                    Ok((None, cas))
                }
            }
            Status::NotFound => Ok((None, None)),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_shared_data(
        key_data: *const u8,
        key_size: usize,
        value_data: *const u8,
        value_size: usize,
        cas: u32,
    ) -> Status;
}

pub fn set_shared_data(key: &str, value: Option<&[u8]>, cas: Option<u32>) -> Result<(), Status> {
    unsafe {
        match proxy_set_shared_data(
            key.as_ptr(),
            key.len(),
            value.map_or(null(), |value| value.as_ptr()),
            value.map_or(0, |value| value.len()),
            cas.unwrap_or(0),
        ) {
            Status::Ok => Ok(()),
            Status::CasMismatch => Err(Status::CasMismatch),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_register_shared_queue(
        name_data: *const u8,
        name_size: usize,
        return_id: *mut u32,
    ) -> Status;
}

pub fn register_shared_queue(name: &str) -> Result<u32, Status> {
    unsafe {
        let mut return_id: u32 = 0;
        match proxy_register_shared_queue(name.as_ptr(), name.len(), &mut return_id) {
            Status::Ok => Ok(return_id),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_resolve_shared_queue(
        vm_id_data: *const u8,
        vm_id_size: usize,
        name_data: *const u8,
        name_size: usize,
        return_id: *mut u32,
    ) -> Status;
}

pub fn resolve_shared_queue(vm_id: &str, name: &str) -> Result<Option<u32>, Status> {
    let mut return_id: u32 = 0;
    unsafe {
        match proxy_resolve_shared_queue(
            vm_id.as_ptr(),
            vm_id.len(),
            name.as_ptr(),
            name.len(),
            &mut return_id,
        ) {
            Status::Ok => Ok(Some(return_id)),
            Status::NotFound => Ok(None),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_dequeue_shared_queue(
        queue_id: u32,
        return_value_data: *mut *mut u8,
        return_value_size: *mut usize,
    ) -> Status;
}

pub fn dequeue_shared_queue(queue_id: u32) -> Result<Option<Bytes>, Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_dequeue_shared_queue(queue_id, &mut return_data, &mut return_size) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(Vec::from_raw_parts(
                        return_data,
                        return_size,
                        return_size,
                    )))
                } else {
                    Ok(None)
                }
            }
            Status::Empty => Ok(None),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_enqueue_shared_queue(
        queue_id: u32,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

pub fn enqueue_shared_queue(queue_id: u32, value: Option<&[u8]>) -> Result<(), Status> {
    unsafe {
        match proxy_enqueue_shared_queue(
            queue_id,
            value.map_or(null(), |value| value.as_ptr()),
            value.map_or(0, |value| value.len()),
        ) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_continue_stream(stream_type: StreamType) -> Status;
}

pub fn resume_downstream() -> Result<(), Status> {
    unsafe {
        match proxy_continue_stream(StreamType::Downstream) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn resume_upstream() -> Result<(), Status> {
    unsafe {
        match proxy_continue_stream(StreamType::Upstream) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn resume_http_request() -> Result<(), Status> {
    unsafe {
        match proxy_continue_stream(StreamType::HttpRequest) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn resume_http_response() -> Result<(), Status> {
    unsafe {
        match proxy_continue_stream(StreamType::HttpResponse) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_close_stream(stream_type: StreamType) -> Status;
}

pub fn close_downstream() -> Result<(), Status> {
    unsafe {
        match proxy_close_stream(StreamType::Downstream) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}
pub fn close_upstream() -> Result<(), Status> {
    unsafe {
        match proxy_close_stream(StreamType::Upstream) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn reset_http_request() -> Result<(), Status> {
    unsafe {
        match proxy_close_stream(StreamType::HttpRequest) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn reset_http_response() -> Result<(), Status> {
    unsafe {
        match proxy_close_stream(StreamType::HttpResponse) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_send_local_response(
        status_code: u32,
        status_code_details_data: *const u8,
        status_code_details_size: usize,
        body_data: *const u8,
        body_size: usize,
        headers_data: *const u8,
        headers_size: usize,
        grpc_status: i32,
    ) -> Status;
}

pub fn send_http_response(
    status_code: u32,
    headers: Vec<(&str, &str)>,
    body: Option<&[u8]>,
) -> Result<(), Status> {
    let serialized_headers = utils::serialize_map(headers);
    unsafe {
        match proxy_send_local_response(
            status_code,
            null(),
            0,
            body.map_or(null(), |body| body.as_ptr()),
            body.map_or(0, |body| body.len()),
            serialized_headers.as_ptr(),
            serialized_headers.len(),
            -1,
        ) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn send_grpc_response(
    grpc_status: GrpcStatusCode,
    grpc_status_message: Option<&str>,
    custom_metadata: Vec<(&str, &[u8])>,
) -> Result<(), Status> {
    let serialized_custom_metadata = utils::serialize_map_bytes(custom_metadata);
    unsafe {
        match proxy_send_local_response(
            200,
            null(),
            0,
            grpc_status_message.map_or(null(), |grpc_status_message| grpc_status_message.as_ptr()),
            grpc_status_message.map_or(0, |grpc_status_message| grpc_status_message.len()),
            serialized_custom_metadata.as_ptr(),
            serialized_custom_metadata.len(),
            grpc_status as i32,
        ) {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_http_call(
        upstream_data: *const u8,
        upstream_size: usize,
        headers_data: *const u8,
        headers_size: usize,
        body_data: *const u8,
        body_size: usize,
        trailers_data: *const u8,
        trailers_size: usize,
        timeout: u32,
        return_token: *mut u32,
    ) -> Status;
}

pub fn dispatch_http_call(
    upstream: &str,
    headers: Vec<(&str, &str)>,
    body: Option<&[u8]>,
    trailers: Vec<(&str, &str)>,
    timeout: Duration,
) -> Result<u32, Status> {
    let serialized_headers = utils::serialize_map(headers);
    let serialized_trailers = utils::serialize_map(trailers);
    let mut return_token: u32 = 0;
    unsafe {
        match proxy_http_call(
            upstream.as_ptr(),
            upstream.len(),
            serialized_headers.as_ptr(),
            serialized_headers.len(),
            body.map_or(null(), |body| body.as_ptr()),
            body.map_or(0, |body| body.len()),
            serialized_trailers.as_ptr(),
            serialized_trailers.len(),
            timeout.as_millis() as u32,
            &mut return_token,
        ) {
            Status::Ok => {
                dispatcher::register_callout(return_token);
                Ok(return_token)
            }
            Status::BadArgument => Err(Status::BadArgument),
            Status::InternalFailure => Err(Status::InternalFailure),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_grpc_call(
        upstream_data: *const u8,
        upstream_size: usize,
        service_name_data: *const u8,
        service_name_size: usize,
        method_name_data: *const u8,
        method_name_size: usize,
        initial_metadata_data: *const u8,
        initial_metadata_size: usize,
        message_data_data: *const u8,
        message_data_size: usize,
        timeout: u32,
        return_callout_id: *mut u32,
    ) -> Status;
}

pub fn dispatch_grpc_call(
    upstream_name: &str,
    service_name: &str,
    method_name: &str,
    initial_metadata: Vec<(&str, &[u8])>,
    message: Option<&[u8]>,
    timeout: Duration,
) -> Result<u32, Status> {
    let mut return_callout_id = 0;
    let serialized_initial_metadata = utils::serialize_map_bytes(initial_metadata);
    unsafe {
        match proxy_grpc_call(
            upstream_name.as_ptr(),
            upstream_name.len(),
            service_name.as_ptr(),
            service_name.len(),
            method_name.as_ptr(),
            method_name.len(),
            serialized_initial_metadata.as_ptr(),
            serialized_initial_metadata.len(),
            message.map_or(null(), |message| message.as_ptr()),
            message.map_or(0, |message| message.len()),
            timeout.as_millis() as u32,
            &mut return_callout_id,
        ) {
            Status::Ok => {
                dispatcher::register_grpc_callout(return_callout_id);
                Ok(return_callout_id)
            }
            Status::ParseFailure => Err(Status::ParseFailure),
            Status::InternalFailure => Err(Status::InternalFailure),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_grpc_stream(
        upstream_data: *const u8,
        upstream_size: usize,
        service_name_data: *const u8,
        service_name_size: usize,
        method_name_data: *const u8,
        method_name_size: usize,
        initial_metadata_data: *const u8,
        initial_metadata_size: usize,
        return_stream_id: *mut u32,
    ) -> Status;
}

pub fn open_grpc_stream(
    upstream_name: &str,
    service_name: &str,
    method_name: &str,
    initial_metadata: Vec<(&str, &[u8])>,
) -> Result<u32, Status> {
    let mut return_stream_id = 0;
    let serialized_initial_metadata = utils::serialize_map_bytes(initial_metadata);
    unsafe {
        match proxy_grpc_stream(
            upstream_name.as_ptr(),
            upstream_name.len(),
            service_name.as_ptr(),
            service_name.len(),
            method_name.as_ptr(),
            method_name.len(),
            serialized_initial_metadata.as_ptr(),
            serialized_initial_metadata.len(),
            &mut return_stream_id,
        ) {
            Status::Ok => {
                dispatcher::register_grpc_stream(return_stream_id);
                Ok(return_stream_id)
            }
            Status::ParseFailure => Err(Status::ParseFailure),
            Status::InternalFailure => Err(Status::InternalFailure),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_grpc_send(
        token: u32,
        message_ptr: *const u8,
        message_len: usize,
        end_stream: bool,
    ) -> Status;
}

pub fn send_grpc_stream_message(
    token: u32,
    message: Option<&[u8]>,
    end_stream: bool,
) -> Result<(), Status> {
    unsafe {
        match proxy_grpc_send(
            token,
            message.map_or(null(), |message| message.as_ptr()),
            message.map_or(0, |message| message.len()),
            end_stream,
        ) {
            Status::Ok => Ok(()),
            Status::BadArgument => Err(Status::BadArgument),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_grpc_cancel(token_id: u32) -> Status;
}

pub fn cancel_grpc_call(token_id: u32) -> Result<(), Status> {
    unsafe {
        match proxy_grpc_cancel(token_id) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

pub fn cancel_grpc_stream(token_id: u32) -> Result<(), Status> {
    unsafe {
        match proxy_grpc_cancel(token_id) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_grpc_close(token_id: u32) -> Status;
}

pub fn close_grpc_stream(token_id: u32) -> Result<(), Status> {
    unsafe {
        match proxy_grpc_close(token_id) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_status(
        return_code: *mut u32,
        return_message_data: *mut *mut u8,
        return_message_size: *mut usize,
    ) -> Status;
}

pub fn get_grpc_status() -> Result<(u32, Option<String>), Status> {
    let mut return_code: u32 = 0;
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_get_status(&mut return_code, &mut return_data, &mut return_size) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok((
                        return_code,
                        Some(
                            String::from_utf8(Vec::from_raw_parts(
                                return_data,
                                return_size,
                                return_size,
                            ))
                            .unwrap(),
                        ),
                    ))
                } else {
                    Ok((return_code, None))
                }
            }
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_set_effective_context(context_id: u32) -> Status;
}

pub fn set_effective_context(context_id: u32) -> Result<(), Status> {
    unsafe {
        match proxy_set_effective_context(context_id) {
            Status::Ok => Ok(()),
            Status::BadArgument => Err(Status::BadArgument),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_call_foreign_function(
        function_name_data: *const u8,
        function_name_size: usize,
        arguments_data: *const u8,
        arguments_size: usize,
        results_data: *mut *mut u8,
        results_size: *mut usize,
    ) -> Status;
}

pub fn call_foreign_function(
    function_name: &str,
    arguments: Option<&[u8]>,
) -> Result<Option<Bytes>, Status> {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;
    unsafe {
        match proxy_call_foreign_function(
            function_name.as_ptr(),
            function_name.len(),
            arguments.map_or(null(), |arguments| arguments.as_ptr()),
            arguments.map_or(0, |arguments| arguments.len()),
            &mut return_data,
            &mut return_size,
        ) {
            Status::Ok => {
                if !return_data.is_null() {
                    Ok(Some(Vec::from_raw_parts(
                        return_data,
                        return_size,
                        return_size,
                    )))
                } else {
                    Ok(None)
                }
            }
            Status::NotFound => Err(Status::NotFound),
            Status::BadArgument => Err(Status::BadArgument),
            Status::SerializationFailure => Err(Status::SerializationFailure),
            Status::InternalFailure => Err(Status::InternalFailure),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_done() -> Status;
}

pub fn done() -> Result<(), Status> {
    unsafe {
        match proxy_done() {
            Status::Ok => Ok(()),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_define_metric(
        metric_type: MetricType,
        name_data: *const u8,
        name_size: usize,
        return_id: *mut u32,
    ) -> Status;
}

pub fn define_metric(metric_type: MetricType, name: &str) -> Result<u32, Status> {
    let mut return_id: u32 = 0;
    unsafe {
        match proxy_define_metric(metric_type, name.as_ptr(), name.len(), &mut return_id) {
            Status::Ok => Ok(return_id),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_get_metric(metric_id: u32, return_value: *mut u64) -> Status;
}

pub fn get_metric(metric_id: u32) -> Result<u64, Status> {
    let mut return_value: u64 = 0;
    unsafe {
        match proxy_get_metric(metric_id, &mut return_value) {
            Status::Ok => Ok(return_value),
            Status::NotFound => Err(Status::NotFound),
            Status::BadArgument => Err(Status::BadArgument),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_record_metric(metric_id: u32, value: u64) -> Status;
}

pub fn record_metric(metric_id: u32, value: u64) -> Result<(), Status> {
    unsafe {
        match proxy_record_metric(metric_id, value) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

extern "C" {
    fn proxy_increment_metric(metric_id: u32, offset: i64) -> Status;
}

pub fn increment_metric(metric_id: u32, offset: i64) -> Result<(), Status> {
    unsafe {
        match proxy_increment_metric(metric_id, offset) {
            Status::Ok => Ok(()),
            Status::NotFound => Err(Status::NotFound),
            Status::BadArgument => Err(Status::BadArgument),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

mod utils {
    use crate::types::Bytes;
    use std::convert::TryFrom;

    pub(super) fn serialize_property_path(path: Vec<&str>) -> Bytes {
        if path.is_empty() {
            return Vec::new();
        }
        let mut size: usize = 0;
        for part in &path {
            size += part.len() + 1;
        }
        let mut bytes: Bytes = Vec::with_capacity(size);
        for part in &path {
            bytes.extend_from_slice(part.as_bytes());
            bytes.push(0);
        }
        bytes.pop();
        bytes
    }

    pub(super) fn serialize_map(map: Vec<(&str, &str)>) -> Bytes {
        let mut size: usize = 4;
        for (name, value) in &map {
            size += name.len() + value.len() + 10;
        }
        let mut bytes: Bytes = Vec::with_capacity(size);
        bytes.extend_from_slice(&map.len().to_le_bytes());
        for (name, value) in &map {
            bytes.extend_from_slice(&name.len().to_le_bytes());
            bytes.extend_from_slice(&value.len().to_le_bytes());
        }
        for (name, value) in &map {
            bytes.extend_from_slice(name.as_bytes());
            bytes.push(0);
            bytes.extend_from_slice(value.as_bytes());
            bytes.push(0);
        }
        bytes
    }

    pub(super) fn serialize_map_bytes(map: Vec<(&str, &[u8])>) -> Bytes {
        let mut size: usize = 4;
        for (name, value) in &map {
            size += name.len() + value.len() + 10;
        }
        let mut bytes: Bytes = Vec::with_capacity(size);
        bytes.extend_from_slice(&map.len().to_le_bytes());
        for (name, value) in &map {
            bytes.extend_from_slice(&name.len().to_le_bytes());
            bytes.extend_from_slice(&value.len().to_le_bytes());
        }
        for (name, value) in &map {
            bytes.extend_from_slice(name.as_bytes());
            bytes.push(0);
            bytes.extend_from_slice(value);
            bytes.push(0);
        }
        bytes
    }

    pub(super) fn deserialize_map(bytes: &[u8]) -> Vec<(String, String)> {
        let mut map = Vec::new();
        if bytes.is_empty() {
            return map;
        }
        let size = u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[0..4]).unwrap()) as usize;
        let mut p = 4 + size * 8;
        for n in 0..size {
            let s = 4 + n * 8;
            let size = u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[s..s + 4]).unwrap()) as usize;
            let key = bytes[p..p + size].to_vec();
            p += size + 1;
            let size =
                u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[s + 4..s + 8]).unwrap()) as usize;
            let value = bytes[p..p + size].to_vec();
            p += size + 1;
            map.push((
                String::from_utf8(key).unwrap(),
                String::from_utf8(value).unwrap(),
            ));
        }
        map
    }

    pub(super) fn deserialize_map_bytes(bytes: &[u8]) -> Vec<(String, Bytes)> {
        let mut map = Vec::new();
        if bytes.is_empty() {
            return map;
        }
        let size = u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[0..4]).unwrap()) as usize;
        let mut p = 4 + size * 8;
        for n in 0..size {
            let s = 4 + n * 8;
            let size = u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[s..s + 4]).unwrap()) as usize;
            let key = bytes[p..p + size].to_vec();
            p += size + 1;
            let size =
                u32::from_le_bytes(<[u8; 4]>::try_from(&bytes[s + 4..s + 8]).unwrap()) as usize;
            let value = bytes[p..p + size].to_vec();
            p += size + 1;
            map.push((String::from_utf8(key).unwrap(), value));
        }
        map
    }
}
