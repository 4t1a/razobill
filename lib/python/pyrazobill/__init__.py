"""
pyrazobill: High-Performance Log Parsing & Information-Theoretic Storage Engine
=================================================================================

A production-grade, lossless log parser and semantic information accounting engine
grounded in "Latitude and Doubt" (M. M. Atta, 2026).

Key Capabilities:
-----------------
1. Zero-Allocation Parsing: parse() and parseBatch()
2. Lossless Invertibility: reconstruct(record) == original_message
3. Information Theory: entropy(), ledger calculations
4. Multi-Core Scaling: auditParallel() utilizing all CPU cores without GIL contention
5. Direct SQL Engine Interop: extensionPath() for native DuckDB integration
"""

import os
import sys
from pathlib import Path

# BUGFIX & WINDOWS ARCHITECTURE NOTE:
# Starting in Python 3.8 on Windows, CPython stopped looking in PATH for DLL dependencies
# of C-extension modules (.pyd). If a user has a MinGW-compiled extension, it depends on
# runtime libraries like `libwinpthread-1.dll`. Without explicitly calling os.add_dll_directory(),
# Python throws an obscure "ImportError: DLL load failed: The specified module could not be found."
# We automatically inspect the package directory and system PATH to register valid directories,
# giving users a seamless "pip install && import pyrazobill" experience without manual DLL copying.
_pkg_dir = Path(__file__).resolve().parent
if sys.platform == "win32":
    try:
        os.add_dll_directory(str(_pkg_dir))
    except Exception:
        pass
    for _p in os.environ.get("PATH", "").split(os.pathsep):
        if _p and os.path.isdir(_p):
            try:
                os.add_dll_directory(_p)
            except Exception:
                pass

try:
    from .Core import (
        ScanMode,
        SlotRecord,
        AuditResult,
        parse,
        parseBatch,
        decompose,
        decomposeBatch,
        reconstruct,
        formKey,
        templateOf,
        hash64,
        entropy,
        auditParallel,
        extensionPath,
        version,
        abiVersion,
        versionInfo,
        Cell,
        XY,
        FormMeaning,
        Ledger,
        PriceOfSaying,
        HierarchicalStep,
        DepthLedger,
        ConsequenceDecomposition,
        computeLedger,
        priceOfSaying,
        computeDepthLedger,
        consequenceDecomposition,
        millerMadow,
        certifiedInterval,
        SynonymousOption,
        MeaningChoice,
        ChannelCapacity,
        EmbedResult,
        DecodeResult,
        channelAnalyzeCapacity,
        channelRequiredKeyBits,
        channelFailureProbabilityBound,
        channelExactFailureProbability,
        channelEmbed,
        channelDecode,
    )
    _NATIVE = True
except ImportError:
    from ._fallback import (
        ScanMode,
        SlotRecord,
        AuditResult,
        parse,
        parseBatch,
        decompose,
        decomposeBatch,
        reconstruct,
        formKey,
        templateOf,
        hash64,
        entropy,
        auditParallel,
        extensionPath,
        version,
        abiVersion,
        versionInfo,
        Cell,
        XY,
        FormMeaning,
        Ledger,
        PriceOfSaying,
        HierarchicalStep,
        DepthLedger,
        ConsequenceDecomposition,
        computeLedger,
        priceOfSaying,
        computeDepthLedger,
        consequenceDecomposition,
        millerMadow,
        certifiedInterval,
        SynonymousOption,
        MeaningChoice,
        ChannelCapacity,
        EmbedResult,
        DecodeResult,
        channelAnalyzeCapacity,
        channelRequiredKeyBits,
        channelFailureProbabilityBound,
        channelExactFailureProbability,
        channelEmbed,
        channelDecode,
    )
    _NATIVE = False

__version__ = version()
__version_info__ = versionInfo()
__abi_version__ = abiVersion()

__all__ = [
    "ScanMode",
    "SlotRecord",
    "AuditResult",
    "parse",
    "parseBatch",
    "decompose",
    "decomposeBatch",
    "reconstruct",
    "formKey",
    "templateOf",
    "hash64",
    "entropy",
    "auditParallel",
    "extensionPath",
    "version",
    "abiVersion",
    "versionInfo",
    "Cell",
    "XY",
    "FormMeaning",
    "Ledger",
    "PriceOfSaying",
    "HierarchicalStep",
    "DepthLedger",
    "ConsequenceDecomposition",
    "computeLedger",
    "priceOfSaying",
    "computeDepthLedger",
    "consequenceDecomposition",
    "millerMadow",
    "certifiedInterval",
    "SynonymousOption",
    "MeaningChoice",
    "ChannelCapacity",
    "EmbedResult",
    "DecodeResult",
    "channelAnalyzeCapacity",
    "channelRequiredKeyBits",
    "channelFailureProbabilityBound",
    "channelExactFailureProbability",
    "channelEmbed",
    "channelDecode",
]
