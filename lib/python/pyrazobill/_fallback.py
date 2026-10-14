"""
Pure-Python fallback implementation for pyrazobill.
Provides 100% equivalent functionality when the native C++20 extension is unavailable.
"""

from typing import Any, List, Sequence, Tuple, Union

DELIMITERS = set("\t\n\x0c\r =,;:()[]{}\"'<>|/")
CONTEXT_KEYWORDS = {
    "for",
    "user",
    "by",
    "from",
    "ruser",
    "rhost",
    "host",
    "account",
    "client",
    "src",
    "dest",
    "to",
    "at",
    "proxy",
    "server",
    "node",
    "target",
    "app",
    "device",
}


def _is_int_token(s: str) -> bool:
    if not s or len(s) > 18:
        return False
    if s[0] == "0":
        return len(s) == 1
    return all("0" <= c <= "9" for c in s)


def _scan(msg: str, mode: str):
    n = len(msg)
    p = 0
    gap = 0
    last_word = ""
    unclosed_dquote_end = -1
    unclosed_squote_end = -1

    while p < n:
        d_start = p
        while p < n and msg[p] in DELIMITERS and not (mode == "semantic" and msg[p] in ('"', "'")):
            p += 1
        d_end = p
        last_delim = msg[d_end - 1] if d_end > d_start else ""

        if p >= n:
            break

        if mode == "semantic" and msg[p] in ('"', "'"):
            q = msg[p]
            if (q == '"' and p < unclosed_dquote_end) or (q == "'" and p < unclosed_squote_end):
                pass
            else:
                q_start = p
                p += 1
                while p < n and msg[p] != q and msg[p] != "\n":
                    if msg[p] == "\\" and p + 1 < n:
                        p += 2
                    else:
                        p += 1
                if p < n and msg[p] == q:
                    p += 1
                    tok = msg[q_start:p]
                    yield msg[gap:q_start], tok
                    gap = p
                    last_word = ""
                    continue
                else:
                    if q == '"':
                        unclosed_dquote_end = p
                    else:
                        unclosed_squote_end = p
                    p = q_start + 1

        start = p
        seen_digit = False
        while p < n and msg[p] not in DELIMITERS:
            if "0" <= msg[p] <= "9":
                seen_digit = True
            p += 1

        tok = msg[start:p]
        is_var = False

        if seen_digit:
            is_var = True
        elif mode == "semantic":
            if last_delim in ("=", "@", '"', "'"):
                is_var = True
            elif last_word and last_word.lower() in CONTEXT_KEYWORDS:
                is_var = True
            elif p < n and msg[p] == ":" and "." in tok:
                is_var = True
            elif len(tok) > 4 and (tok.endswith(".exe") or tok.endswith(".dll") or tok.endswith(".so")):
                is_var = True
            elif tok.startswith("/") or "/" in tok:
                is_var = True
            elif len(tok) >= 8 and all(c in "0123456789abcdefABCDEF" for c in tok):
                is_var = True

        if is_var:
            yield msg[gap:start], tok
            gap = p
            last_word = ""
        else:
            last_word = tok

    if gap < n:
        yield msg[gap:n], None


def _escape_braces(s: str) -> str:
    return s.replace("{", "{{").replace("}", "}}")


def split(input_val: Union[str, bytes], mode: str = "semantic") -> Tuple[Union[str, bytes], List[int], List[Any]]:
    if mode not in ("semantic", "exact"):
        raise ValueError(
            f"[razobill] What happened: Invalid scan mode. "
            f"Why: Expected 'semantic' or 'exact', got '{mode}'. "
            f"What to do: Pass mode='semantic' (default) or mode='exact'."
        )

    is_bytes = isinstance(input_val, bytes)
    if is_bytes:
        msg_str = input_val.decode("latin1")
    elif isinstance(input_val, str):
        msg_str = input_val
    else:
        raise TypeError(f"Expected str or bytes, got {type(input_val).__name__}")

    if not msg_str:
        return (b"" if is_bytes else "", [], [])

    tmpl_parts = []
    ints: List[int] = []
    strs: List[Any] = []

    for gap, tok in _scan(msg_str, mode):
        tmpl_parts.append(_escape_braces(gap))
        if tok is not None:
            if _is_int_token(tok):
                tmpl_parts.append("{i}")
                ints.append(int(tok))
            else:
                tmpl_parts.append("{s}")
                if is_bytes:
                    strs.append(tok.encode("latin1"))
                else:
                    strs.append(tok)

    tmpl_str = "".join(tmpl_parts)
    tmpl_res: Union[str, bytes] = tmpl_str.encode("latin1") if is_bytes else tmpl_str
    return (tmpl_res, ints, strs)


def template(input_val: Union[str, bytes], mode: str = "semantic") -> Union[str, bytes]:
    t, _, _ = split(input_val, mode)
    return t


def join(template_val: Union[str, bytes], ints: Sequence[int], strs: Sequence[Union[str, bytes]]) -> Union[str, bytes]:
    is_bytes = isinstance(template_val, bytes)
    if is_bytes:
        tmpl_str = template_val.decode("latin1")
        str_tokens = [s.decode("latin1") if isinstance(s, bytes) else str(s) for s in strs]
    elif isinstance(template_val, str):
        tmpl_str = template_val
        str_tokens = [s.decode("utf-8", "surrogateescape") if isinstance(s, bytes) else str(s) for s in strs]
    else:
        raise TypeError(f"Expected str or bytes template, got {type(template_val).__name__}")

    out_parts = []
    i_idx = 0
    s_idx = 0
    p = 0
    n = len(tmpl_str)

    while p < n:
        c = tmpl_str[p]
        if c == "{":
            if p + 1 < n and tmpl_str[p + 1] == "{":
                out_parts.append("{")
                p += 2
            elif p + 2 < n and tmpl_str[p + 1] == "i" and tmpl_str[p + 2] == "}":
                if i_idx >= len(ints):
                    raise ValueError(
                        "[razobill] What happened: Failed to join log components. "
                        "Why: Count mismatch (template has more {i} placeholders than provided ints). "
                        "What to do: Verify that {i} and {s} placeholders match provided values."
                    )
                out_parts.append(str(ints[i_idx]))
                i_idx += 1
                p += 3
            elif p + 2 < n and tmpl_str[p + 1] == "s" and tmpl_str[p + 2] == "}":
                if s_idx >= len(str_tokens):
                    raise ValueError(
                        "[razobill] What happened: Failed to join log components. "
                        "Why: Count mismatch (template has more {s} placeholders than provided strs). "
                        "What to do: Verify that {i} and {s} placeholders match provided values."
                    )
                out_parts.append(str_tokens[s_idx])
                s_idx += 1
                p += 3
            else:
                raise ValueError(
                    f"[razobill] What happened: Malformed template syntax. "
                    f"Why: Unclosed or unrecognized placeholder near index {p}. "
                    f"What to do: Use {{i}}, {{s}}, or escape literal braces as '{{{{' or '}}}}'."
                )
        elif c == "}":
            if p + 1 < n and tmpl_str[p + 1] == "}":
                out_parts.append("}")
                p += 2
            else:
                raise ValueError(
                    f"[razobill] What happened: Malformed template syntax. "
                    f"Why: Unexpected single '}}' at index {p}. "
                    f"What to do: Escape literal braces as '}}}}'."
                )
        else:
            out_parts.append(c)
            p += 1

    if i_idx != len(ints) or s_idx != len(strs):
        raise ValueError(
            f"[razobill] What happened: Failed to join log components. "
            f"Why: Count mismatch (provided {len(ints)} ints, {len(strs)} strs, but template used {i_idx} ints, {s_idx} strs). "
            f"What to do: Verify that template placeholders match provided values."
        )

    res_str = "".join(out_parts)
    return res_str.encode("latin1") if is_bytes else res_str


def split_many(
    inputs: Sequence[Union[str, bytes]], mode: str = "semantic"
) -> List[Tuple[Union[str, bytes], List[int], List[Any]]]:
    return [split(item, mode) for item in inputs]


def version() -> str:
    return "2.0.0"
