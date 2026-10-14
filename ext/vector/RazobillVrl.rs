//! Native Vector Remap Language (VRL) function wrapping librazobill Universal C ABI.
//!
//! Provides zero-copy parsing of raw log strings into (template, ints, strs) structs.

use std::collections::BTreeMap;
use std::ffi::CString;
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
    let c_str = CString::new(raw_msg).map_err(|e| e.to_string())?;

    let status = unsafe {
        RzSplit(
            c_str.as_ptr(),
            raw_msg.len(),
            &mut record,
        )
    };

    if !matches!(status, RzStatus::Ok) {
        return Err("Razobill decomposition failed".to_string());
    }

    let form_slice =
        unsafe { std::slice::from_raw_parts(record.template_str as *const u8, record.template_length) };
    let form_str = String::from_utf8_lossy(form_slice).to_string();

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
            let slice =
                unsafe { std::slice::from_raw_parts(str_ptrs[i] as *const u8, str_lens[i]) };
            strs.push(serde_json::Value::from(
                String::from_utf8_lossy(slice).to_string(),
            ));
        }
    }

    unsafe {
        RzFree(&mut record);
    }

    let mut map = BTreeMap::new();
    map.insert("form".to_string(), serde_json::Value::from(form_str));
    map.insert("ints".to_string(), serde_json::Value::Array(ints));
    map.insert("strs".to_string(), serde_json::Value::Array(strs));

    Ok(map)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_razobill() {
        let msg = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2";
        let res = parse_razobill(msg).expect("Failed to parse log record");
        assert!(res.contains_key("form"));
        assert!(res.contains_key("ints"));
        assert!(res.contains_key("strs"));

        let ints = res.get("ints").unwrap().as_array().unwrap();
        assert_eq!(ints.len(), 1);
        assert_eq!(ints[0].as_i64().unwrap(), 54322);

        let strs = res.get("strs").unwrap().as_array().unwrap();
        assert_eq!(strs.len(), 3);
        assert_eq!(strs[0].as_str().unwrap(), "ubuntu");
        assert_eq!(strs[1].as_str().unwrap(), "192.168.1.100");
        assert_eq!(strs[2].as_str().unwrap(), "ssh2");
    }
}
