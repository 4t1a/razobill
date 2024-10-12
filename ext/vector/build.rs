use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let build_dir = manifest_dir
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("build");

    // Instruct Cargo to link against razobill library
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=dylib=razobill");

    // On Windows, copy razobill.dll into target and target/deps so tests find the DLL automatically
    if cfg!(target_os = "windows") {
        let dll_src = build_dir.join("razobill.dll");
        if dll_src.exists() {
            if let Ok(out_dir) = env::var("OUT_DIR") {
                let out_path = PathBuf::from(out_dir);
                // out_path is target/{profile}/build/{crate-hash}/out
                // ancestors: 0=out, 1=crate-hash, 2=build, 3=debug/release
                if let Some(target_profile_dir) = out_path.ancestors().nth(3) {
                    let _ = fs::copy(&dll_src, target_profile_dir.join("razobill.dll"));
                    let deps_dir = target_profile_dir.join("deps");
                    let _ = fs::create_dir_all(&deps_dir);
                    let _ = fs::copy(&dll_src, deps_dir.join("razobill.dll"));
                }
            }
        }
    }

    // Track dependencies for incremental builds
    println!(
        "cargo:rerun-if-changed={}",
        build_dir.join("razobill.lib").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        build_dir.join("razobill.dll").display()
    );
}
