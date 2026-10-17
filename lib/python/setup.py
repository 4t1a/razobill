"""
setup.py for pyrazobill

High-performance C++20 Python bindings for Razobill.
Compiles the native pyrazobill.Core extension module from C++ source files,
with resilient pure-Python fallback when compilers/SDKs are absent.
"""

import sys
import os
import glob
from pathlib import Path
from setuptools import setup

baseDir = Path(__file__).resolve().parent
repoRoot = baseDir.parent.parent

# Monorepo single-source-of-truth vs standalone sdist distribution
if (repoRoot / "src").exists() and (repoRoot / "include").exists():
    sources = [
        "PyRazobill.cpp",
        os.path.relpath(repoRoot / "src" / "Slots.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "Stats.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "Channel.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "Dialect.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "BackendAdbc.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "Razobill.cpp", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "src" / "RazobillC.cpp", baseDir).replace("\\", "/"),
    ]
    includeDirs = [
        os.path.relpath(repoRoot / "include", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "thirdparty" / "adbc", baseDir).replace("\\", "/"),
        os.path.relpath(repoRoot / "thirdparty" / "duckdbcapi", baseDir).replace("\\", "/"),
    ]
else:
    # Standalone sdist package using staged directories
    sources = [
        "PyRazobill.cpp",
        "srcCore/Slots.cpp",
        "srcCore/Stats.cpp",
        "srcCore/Channel.cpp",
        "srcCore/Dialect.cpp",
        "srcCore/BackendAdbc.cpp",
        "srcCore/Razobill.cpp",
        "srcCore/RazobillC.cpp",
    ]
    includeDirs = [
        "includeCore",
        "thirdpartyCore/adbc",
        "thirdpartyCore/duckdbcapi",
    ]


def find_windows_sdk_dirs():
    inc_dirs = []
    lib_dirs = []
    if sys.platform == "win32":
        pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        kits_inc = os.path.join(pf86, "Windows Kits", "10", "Include")
        if os.path.exists(kits_inc):
            versions = sorted(glob.glob(os.path.join(kits_inc, "10.*")), reverse=True)
            if versions:
                latest = versions[0]
                for sub in ("ucrt", "shared", "um", "winrt"):
                    p = os.path.join(latest, sub)
                    if os.path.exists(p):
                        inc_dirs.append(p)
        kits_lib = os.path.join(pf86, "Windows Kits", "10", "Lib")
        if os.path.exists(kits_lib):
            versions = sorted(glob.glob(os.path.join(kits_lib, "10.*")), reverse=True)
            if versions:
                latest = versions[0]
                arch = "x64" if sys.maxsize > 2**32 else "x86"
                for sub in ("ucrt", "um"):
                    p = os.path.join(latest, sub, arch)
                    if os.path.exists(p):
                        lib_dirs.append(p)
    return inc_dirs, lib_dirs


try:
    from pybind11.setup_helpers import Pybind11Extension, build_ext

    class CustomBuildExt(build_ext):
        def build_extensions(self):
            compilerType = self.compiler.compiler_type
            inc_dirs, lib_dirs = find_windows_sdk_dirs()
            for ext in self.extensions:
                if compilerType == "msvc":
                    ext.extra_compile_args.extend(["/std:c++20", "/arch:AVX2", "/EHsc", "/bigobj"])
                    ext.include_dirs.extend(inc_dirs)
                    ext.library_dirs.extend(lib_dirs)
                else:
                    ext.extra_compile_args.extend(["-std=c++20", "-march=native", "-fvisibility=hidden"])
                    if sys.platform != "win32":
                        ext.extra_link_args.append("-lpthread")
            try:
                super().build_extensions()
            except Exception as exc:
                print(f"\n[pyrazobill] NOTICE: Native C++20 extension build failed ({exc}).")
                print("[pyrazobill] Proceeding with pure-Python fallback implementation.\n")
                self.extensions = []

    extModules = [
        Pybind11Extension(
            "pyrazobill.Core",
            sources=sources,
            include_dirs=includeDirs,
            cxx_std=20,
        ),
    ]
    cmdClass = {"build_ext": CustomBuildExt}
except ImportError:
    extModules = []
    cmdClass = {}

setup(
    version="1.1.2",
    ext_modules=extModules,
    cmdclass=cmdClass,
)
