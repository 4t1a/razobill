//! Native Vector Remap Language (VRL) function wrapping librazobill Universal C ABI.
//!
//! Provides zero-copy parsing of raw log strings into (template, ints, strs) structs.

use std::collections::BTreeMap;
use std::os::raw::c_char;

#[repr(C)]
#[derive(Debug)]
pub struct RzRecord {
    pub template_str: *const c_char,
    pub template_length: usize,
    pub int_values: *const i64,
    pub int_count: usize,
    pub str_values: *const *const c_char,
    pub str_lengths: *const usize,
    pub str_count: usize,
    pub internal_handle: *mut std::ffi::c_void,
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq)]
pub enum RzStatus {
    Ok = 0,
    ErrInvalidArgument = 1,
    ErrBufferTooSmall = 2,
    ErrMalformedTemplate = 3,
    ErrCountMismatch = 4,
    ErrMessageTooLarge = 5,
    ErrOutOfMemory = 6,
    ErrInternal = 7,
}

extern "C" {
    pub fn RzSplit(
        message: *const c_char,
        message_length: usize,
        out_record: *mut RzRecord,
    ) -> RzStatus;

    pub fn RzFree(record: *mut RzRecord);
}

/// Decomposes an input string into a structured JSON-compatible map.
pub fn parse_razobill(raw_msg: &str) -> Result<BTreeMap<String, serde_json::Value>, String> {
    let mut record: RzRecord = unsafe { std::mem::zeroed() };

    let status = unsafe {
        RzSplit(
            raw_msg.as_ptr() as *const c_char,
            raw_msg.len(),
            &mut record,
        )
    };

    if !matches!(status, RzStatus::Ok) {
        return Err(format!("Razobill decomposition failed with status {:?}", status));
    }

    struct RecordGuard<'a>(&'a mut RzRecord);
    impl<'a> Drop for RecordGuard<'a> {
        fn drop(&mut self) {
            unsafe {
                RzFree(self.0);
            }
        }
    }
    let _guard = RecordGuard(&mut record);

    // DESIGN DECISION (F-EXT-06):
    // Vector VRL represents internal event values as UTF-8 JSON objects (`serde_json::Value`).
    // Using `String::from_utf8_lossy` ensures invalid UTF-8 sequences in raw binary payloads do not panic
    // or fail the entire VRL pipeline, safely replacing malformed bytes with U+FFFD (REPLACEMENT CHARACTER).
    let form_str = if record.template_length > 0 && !record.template_str.is_null() {
        let form_slice = unsafe {
            std::slice::from_raw_parts(record.template_str as *const u8, record.template_length)
        };
        String::from_utf8_lossy(form_slice).to_string()
    } else {
        String::new()
    };

    let mut ints = Vec::with_capacity(record.int_count);
    if record.int_count > 0 && !record.int_values.is_null() {
        let int_slice = unsafe { std::slice::from_raw_parts(record.int_values, record.int_count) };
        for &val in int_slice {
            ints.push(serde_json::Value::from(val));
        }
    }

    let mut strs = Vec::with_capacity(record.str_count);
    if record.str_count > 0 && !record.str_values.is_null() && !record.str_lengths.is_null() {
        let str_ptrs = unsafe { std::slice::from_raw_parts(record.str_values, record.str_count) };
        let str_lens = unsafe { std::slice::from_raw_parts(record.str_lengths, record.str_count) };
        for i in 0..record.str_count {
            if str_lens[i] > 0 && !str_ptrs[i].is_null() {
                let slice =
                    unsafe { std::slice::from_raw_parts(str_ptrs[i] as *const u8, str_lens[i]) };
                strs.push(serde_json::Value::from(
                    String::from_utf8_lossy(slice).to_string(),
                ));
            } else {
                strs.push(serde_json::Value::from(""));
            }
        }
    }

    let mut map = BTreeMap::new();
    map.insert("form".to_string(), serde_json::Value::from(form_str));
    map.insert("ints".to_string(), serde_json::Value::Array(ints));
    map.insert("strs".to_string(), serde_json::Value::Array(strs));

    Ok(map)
}
