use razobill_vector::parse_razobill;

#[test]
fn test_parse_razobill() {
    let msg = "Accepted publickey for ubuntu from 192.168.1.100 port 54322 ssh2";
    let res = parse_razobill(msg).expect("Failed to parse log record");
    assert!(res.contains_key("template"));
    assert!(res.contains_key("form"));
    assert_eq!(res.get("template"), res.get("form"));
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
