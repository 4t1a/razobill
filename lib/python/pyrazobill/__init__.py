"""
Razobill: High-performance log parser and lossless line reconstruction engine.
==============================================================================

Split a log line into a template plus its numbers and words, and join them
back to the exact original bytes or string.

Quick Start:
------------
>>> import pyrazobill as rz
>>> line = "Failed password for root from 192.168.1.100 port 22"
>>> rec = rz.RzSplit(line)
>>> rec.template
'Failed password for {s} from {s} port {i}'
>>> rec.ints
[22]
>>> rec.strs
['root', '192.168.1.100']
>>> rz.RzJoin(*rec) == line
True
"""

import os
import sys
from pathlib import Path
from typing import Any, Iterable, List, NamedTuple, Optional, Sequence, Union

# On Windows, add the package directory and system PATH to DLL search path for compiled extensions
_pkg_dir = Path(__file__).resolve().parent
if sys.platform == "win32":
    try:
        os.add_dll_directory(str(_pkg_dir))
    except (AttributeError, OSError, ValueError):
        pass
    for _p in os.environ.get("PATH", "").split(os.pathsep):
        if _p and os.path.isdir(_p):
            try:
                os.add_dll_directory(_p)
            except (AttributeError, OSError, ValueError):
                pass


class RzError(ValueError):
    """Raised when an operation fails due to malformed input, invalid syntax, or mismatched arguments."""
    pass


class RzRecord(NamedTuple):
    """Decomposed log record containing a template and its variable parameters."""
    template: Union[str, bytes]
    ints: List[int]
    strs: List[Union[str, bytes]]

    def __repr__(self) -> str:
        t_repr = repr(self.template)
        if len(t_repr) > 60:
            t_repr = t_repr[:57] + "..."
        return f"<razobill.RzRecord template={t_repr} ints={len(self.ints)} strs={len(self.strs)}>"


try:
    from . import Core as _core
    _NATIVE = True
except ImportError:
    from . import _fallback as _core
    _NATIVE = False


def isNative() -> bool:
    """Return True if the native C++ SIMD engine is loaded."""
    return _NATIVE


def RzSplit(line: Union[str, bytes], mode: str = "semantic") -> RzRecord:
    """Split a log line into a template plus its numbers and words.

    Parameters
    ----------
    line : str or bytes
        The log line to split. Empty string or bytes are valid.
    mode : {"semantic", "exact"}, default="semantic"
        Token scanning behavior. "semantic" groups variable tokens (such as IPs,
        paths, and usernames) into string placeholders; "exact" isolates numbers.

    Returns
    -------
    RzRecord
        Named tuple of (template, ints, strs) where template contains typed
        placeholders ({i} for integers, {s} for strings). Returns matching type
        (str for str input, bytes for bytes input).

    Examples
    --------
    >>> import pyrazobill as rz
    >>> rec = rz.RzSplit("Failed password for root from 192.168.1.100 port 22")
    >>> rec.template
    'Failed password for {s} from {s} port {i}'
    >>> rec.ints
    [22]
    >>> rec.strs
    ['root', '192.168.1.100']
    """
    if not isinstance(line, (str, bytes)):
        raise RzError(
            f"[razobill] What happened: Unsupported input type. "
            f"Why: Expected str or bytes, got {type(line).__name__}. "
            f"What to do: Pass a str or bytes object. "
            f"Offending input: {repr(line)[:60]}."
        )
    if mode not in ("semantic", "exact"):
        raise RzError(
            f"[razobill] What happened: Invalid scan mode. "
            f"Why: Expected 'semantic' or 'exact', got '{mode}'. "
            f"What to do: Use mode='semantic' (default) or mode='exact'. "
            f"Offending input: '{mode}'."
        )
    try:
        t, i, s = _core.split(line, mode)
        return RzRecord(t, i, s)
    except RzError:
        raise
    except (ValueError, TypeError) as exc:
        raise RzError(str(exc)) from exc


def RzJoin(
    template: Union[str, bytes, RzRecord],
    ints: Optional[Sequence[int]] = None,
    strs: Optional[Sequence[Union[str, bytes]]] = None,
) -> Union[str, bytes]:
    """Join a template and its numbers and words back to the exact original bytes or string.

    Guarantees:
        RzJoin(*RzSplit(line)) == line
        RzJoin(RzSplit(line)) == line

    Parameters
    ----------
    template : str, bytes, or RzRecord
        The template containing {i} and {s} placeholders, or an RzRecord.
    ints : sequence of int, optional
        Integers corresponding to {i} placeholders.
    strs : sequence of str or bytes, optional
        Strings corresponding to {s} placeholders.

    Returns
    -------
    str or bytes
        The reconstructed line, matching the type of the template.

    Examples
    --------
    >>> import pyrazobill as rz
    >>> line = "Failed password for root from 192.168.1.100 port 22"
    >>> rec = rz.RzSplit(line)
    >>> rz.RzJoin(*rec) == line
    True
    """
    if hasattr(template, "template") and hasattr(template, "ints") and hasattr(template, "strs"):
        template, ints, strs = template.template, template.ints, template.strs
    elif isinstance(template, tuple) and len(template) == 3 and ints is None and strs is None:
        template, ints, strs = template[0], template[1], template[2]

    if ints is None:
        ints = []
    if strs is None:
        strs = []

    if not isinstance(template, (str, bytes)):
        raise RzError(
            f"[razobill] What happened: Unsupported template type. "
            f"Why: Expected str or bytes, got {type(template).__name__}. "
            f"What to do: Pass a str, bytes, or RzRecord object."
        )

    try:
        return _core.join(template, ints, strs)
    except RzError:
        raise
    except (ValueError, TypeError) as exc:
        raise RzError(str(exc)) from exc


def RzTemplate(line: Union[str, bytes], mode: str = "semantic") -> Union[str, bytes]:
    """Extract only the structural template for a log line without allocating parameter lists.

    Parameters
    ----------
    line : str or bytes
        The log line to extract the template from.
    mode : {"semantic", "exact"}, default="semantic"
        Token scanning behavior.

    Returns
    -------
    str or bytes
        The template with {i} and {s} placeholders.
    """
    if not isinstance(line, (str, bytes)):
        raise RzError(
            f"[razobill] What happened: Unsupported input type. "
            f"Why: Expected str or bytes, got {type(line).__name__}. "
            f"What to do: Pass a str or bytes object."
        )
    if mode not in ("semantic", "exact"):
        raise RzError(
            f"[razobill] What happened: Invalid scan mode. "
            f"Why: Expected 'semantic' or 'exact', got '{mode}'. "
            f"What to do: Use mode='semantic' (default) or mode='exact'."
        )
    try:
        return _core.template(line, mode)
    except RzError:
        raise
    except (ValueError, TypeError) as exc:
        raise RzError(str(exc)) from exc


def RzSplitMany(lines: Iterable[Union[str, bytes]], mode: str = "semantic") -> List[RzRecord]:
    """Split an iterable of log lines in batch, releasing the GIL across CPU cores.

    Parameters
    ----------
    lines : iterable of str or bytes
        Log lines to split.
    mode : {"semantic", "exact"}, default="semantic"
        Token scanning behavior.

    Returns
    -------
    list of RzRecord
        Decomposed records for all input lines.
    """
    if mode not in ("semantic", "exact"):
        raise RzError(
            f"[razobill] What happened: Invalid scan mode. "
            f"Why: Expected 'semantic' or 'exact', got '{mode}'. "
            f"What to do: Use mode='semantic' (default) or mode='exact'."
        )
    line_list = list(lines)
    for i, line in enumerate(line_list):
        if not isinstance(line, (str, bytes)):
            raise RzError(
                f"[razobill] What happened: Unsupported input type at index {i}. "
                f"Why: Expected str or bytes, got {type(line).__name__}. "
                f"What to do: Ensure all items in lines are str or bytes."
            )
    try:
        raw_records = _core.split_many(line_list, mode)
        return [RzRecord(t, ints, strs) for t, ints, strs in raw_records]
    except RzError:
        raise
    except (ValueError, TypeError) as exc:
        raise RzError(str(exc)) from exc


__version__ = "1.1.1"

__all__ = [
    "RzSplit",
    "RzJoin",
    "RzTemplate",
    "RzSplitMany",
    "RzRecord",
    "RzError",
    "__version__",
]

# Ergonomic aliases
split = RzSplit
join = RzJoin
template = RzTemplate
split_many = RzSplitMany
Record = RzRecord
Error = RzError
