"""
setup.py for pyrazobill

High-performance C++20 Python bindings for Razobill.
Compiles the native pyrazobill.Core extension module from C++ source files.
"""

import sys
import shutil
from pathlib import Path
from setuptools import setup

baseDir = Path(__file__).resolve().parent
repoRoot = baseDir.parent.parent

# Staging directories for self-contained source distributions
srcCoreDir = baseDir / "srcCore"
incCoreDir = baseDir / "includeCore"
thirdpartyCoreDir = baseDir / "thirdpartyCore"

# Sync C++ source and header files from the monorepo root if available
if (repoRoot / "src").exists() and (repoRoot / "include").exists():
    srcCoreDir.mkdir(exist_ok=True)
    for cppFile in (repoRoot / "src").glob("*.cpp"):
        shutil.copy2(cppFile, srcCoreDir / cppFile.name)

    if (incCoreDir / "librazobill").exists():
        shutil.rmtree(incCoreDir / "librazobill")
    incCoreDir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(repoRoot / "include" / "librazobill", incCoreDir / "librazobill")

    if (repoRoot / "thirdparty").exists():
        if thirdpartyCoreDir.exists():
            shutil.rmtree(thirdpartyCoreDir)
        shutil.copytree(repoRoot / "thirdparty", thirdpartyCoreDir)

# All paths must be /-separated and relative to setup.py directory
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

try:
    from pybind11.setup_helpers import Pybind11Extension, build_ext

    class CustomBuildExt(build_ext):
        def build_extensions(self):
            compilerType = self.compiler.compiler_type
            for ext in self.extensions:
                if compilerType == "msvc":
                    ext.extra_compile_args.extend(["/std:c++20", "/arch:AVX2", "/EHsc"])
                else:
                    ext.extra_compile_args.extend(
                        ["-std=c++20", "-march=native", "-fvisibility=hidden"]
                    )
                    if sys.platform != "win32":
                        ext.extra_link_args.append("-lpthread")
            super().build_extensions()

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
    ext_modules=extModules,
    cmdclass=cmdClass,
)
