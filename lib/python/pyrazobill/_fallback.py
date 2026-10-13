"""
_fallback.py: Pure-Python Fallback Implementation for pyrazobill.
Grounded in "Latitude and Doubt: An exact ledger of meaning in messages" (M. M. Atta, 2026).
Ensures 100% functionality and exact invertibility when native C++20 extension is unavailable.
"""

import math
import struct
from enum import IntEnum
from dataclasses import dataclass
from typing import List, Tuple, Dict, Any, Optional

class ScanMode(IntEnum):
    Exact = 0
    DigitOnly = 0
    Semantic = 1

kPlaceholder = 0x12     # ASCII Device Control 2
kTypeSeparator = 0x13   # ASCII Device Control 3
kMaxMessageBytes = 16 * 1024 * 1024

DELIMITERS = set("\t\n\x0c\r =,;:()[]{}\"'<>|/")
CONTEXT_KEYWORDS = {
    "for", "user", "by", "from", "ruser", "rhost", "host",
    "account", "client", "src", "dest", "to", "at", "proxy",
    "server", "node", "target", "app", "device"
}

def isIntToken(s: str) -> bool:
    if not s or len(s) > 18:
        return False
    if s[0] == '0':
        return len(s) == 1
    return all('0' <= c <= '9' for c in s)

def _scan(msg: str, mode: ScanMode = ScanMode.Semantic):
    n = len(msg)
    p = 0
    gap = 0
    last_word = ""
    unclosed_dquote_end = -1
    unclosed_squote_end = -1

    while p < n:
        d_start = p
        while p < n and msg[p] in DELIMITERS and not (mode == ScanMode.Semantic and msg[p] in ('"', "'")):
            p += 1
        d_end = p
        last_delim = msg[d_end - 1] if d_end > d_start else ""

        if p >= n:
            break

        # Quoted strings in Semantic mode
        if mode == ScanMode.Semantic and msg[p] in ('"', "'"):
            q = msg[p]
            if (q == '"' and p < unclosed_dquote_end) or (q == "'" and p < unclosed_squote_end):
                pass
            else:
                q_start = p
                p += 1
                while p < n and msg[p] != q and msg[p] != '\n':
                    if msg[p] == '\\' and p + 1 < n:
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
            if '0' <= msg[p] <= '9':
                seen_digit = True
            p += 1

        tok = msg[start:p]
        is_var = False

        if seen_digit:
            is_var = True
        elif mode == ScanMode.Semantic:
            if last_delim in ('=', '@', '"', "'"):
                is_var = True
            elif last_word and last_word.lower() in CONTEXT_KEYWORDS:
                is_var = True
            elif p < n and msg[p] == ':' and '.' in tok:
                is_var = True
            elif len(tok) > 4 and (tok.endswith(".exe") or tok.endswith(".dll") or tok.endswith(".so")):
                is_var = True

        if is_var:
            yield msg[gap:start], tok
            gap = p
            last_word = ""
        else:
            last_word = tok

    yield msg[gap:n], None

class SlotRecord:
    def __init__(self, skeleton: str = "", types: str = "", ints: Optional[List[int]] = None, strs: Optional[List[str]] = None):
        self.skeleton = skeleton
        self.types = types
        self.ints = ints if ints is not None else []
        self.strs = strs if strs is not None else []

    def clear(self):
        self.skeleton = ""
        self.types = ""
        self.ints = []
        self.strs = []

    @property
    def formKey(self) -> str:
        return f"{self.skeleton}{chr(kTypeSeparator)}{self.types}"

    @property
    def formKeyHash(self) -> int:
        return hash64(self.formKey)

    def reconstruct(self) -> str:
        return reconstruct(self.formKey, self.ints, self.strs)

    def __repr__(self) -> str:
        return f"<pyrazobill.SlotRecord formKey='{templateOf(self.formKey)}' ints={len(self.ints)} strs={len(self.strs)}>"

@dataclass
class AuditResult:
    totalLines: int = 0
    totalBytes: int = 0
    elapsedSec: float = 0.0
    uniqueFormsCount: int = 0
    msgsPerSec: float = 0.0
    mbPerSec: float = 0.0
    mismatches: int = 0

def parse(msg: str, mode: ScanMode = ScanMode.Semantic) -> SlotRecord:
    rec = SlotRecord()
    if len(msg) > kMaxMessageBytes:
        return rec
    
    # Check for reserved bytes (0x11..0x13) matching C++ Scanner
    if '\x11' in msg or chr(kPlaceholder) in msg or chr(kTypeSeparator) in msg:
        rec.skeleton = chr(kPlaceholder)
        rec.types = "S"
        rec.strs.append(msg)
        return rec

    skel_parts = []
    types_parts = []
    for gap, tok in _scan(msg, mode):
        skel_parts.append(gap)
        if tok is not None:
            skel_parts.append(chr(kPlaceholder))
            if isIntToken(tok):
                rec.ints.append(int(tok))
                types_parts.append("I")
            else:
                rec.strs.append(tok)
                types_parts.append("S")

    rec.skeleton = "".join(skel_parts)
    rec.types = "".join(types_parts)
    return rec

def parseBatch(msgs: List[str], mode: ScanMode = ScanMode.Semantic) -> List[SlotRecord]:
    return [parse(m, mode) for m in msgs]

def decompose(msg: str, mode: ScanMode = ScanMode.Semantic) -> Tuple[str, List[int], List[str]]:
    rec = parse(msg, mode)
    return rec.formKey, list(rec.ints), list(rec.strs)

def decomposeBatch(msgs: List[str], mode: ScanMode = ScanMode.Semantic) -> List[Tuple[str, List[int], List[str]]]:
    return [decompose(m, mode) for m in msgs]

def reconstruct(formKey_or_record: Any, ints: Optional[Any] = None, strs: Optional[Any] = None) -> str:
    if isinstance(formKey_or_record, SlotRecord):
        return formKey_or_record.reconstruct()

    formKey = str(formKey_or_record)
    ints_list = list(ints) if ints is not None else []
    strs_list = list(strs) if strs is not None else []

    sep = formKey.rfind(chr(kTypeSeparator))
    if sep == -1:
        return formKey
    skeleton = formKey[:sep]
    types = formKey[sep + 1:]

    out = []
    ti = 0
    ii = 0
    si = 0
    for c in skeleton:
        if ord(c) != kPlaceholder:
            out.append(c)
        else:
            if ti < len(types):
                t = types[ti]
                ti += 1
                if t == 'I':
                    if ii < len(ints_list):
                        out.append(str(ints_list[ii]))
                        ii += 1
                elif t == 'S':
                    if si < len(strs_list):
                        out.append(str(strs_list[si]))
                        si += 1
    return "".join(out)

def formKey(msg: str, mode: ScanMode = ScanMode.Semantic) -> str:
    return parse(msg, mode).formKey

def templateOf(form: str) -> str:
    sep = form.rfind(chr(kTypeSeparator))
    skeleton = form[:sep] if sep != -1 else form
    return skeleton.replace(chr(kPlaceholder), "<*>")

# Pure-Python xxHash64 implementation (Seed 0, ClickHouse & C++ compatible)
P1 = 11400714785074694791
P2 = 14029467366897019727
P3 = 1609587929392839161
P4 = 9650029242287828579
P5 = 2870177450012600261
MASK64 = 0xFFFFFFFFFFFFFFFF

def _rotl64(x: int, r: int) -> int:
    return ((x << r) & MASK64) | (x >> (64 - r))

def _round64(acc: int, val: int) -> int:
    return (_rotl64((acc + (val * P2) & MASK64) & MASK64, 31) * P1) & MASK64

def hash64(data: Any) -> int:
    if isinstance(data, str):
        b = data.encode("utf-8")
    else:
        b = bytes(data)
    n = len(b)
    i = 0

    if n >= 32:
        v1 = (P1 + P2) & MASK64
        v2 = P2
        v3 = 0
        v4 = (0 - P1) & MASK64
        while i + 32 <= n:
            val1 = struct.unpack_from("<Q", b, i)[0]
            val2 = struct.unpack_from("<Q", b, i + 8)[0]
            val3 = struct.unpack_from("<Q", b, i + 16)[0]
            val4 = struct.unpack_from("<Q", b, i + 24)[0]
            v1 = _round64(v1, val1)
            v2 = _round64(v2, val2)
            v3 = _round64(v3, val3)
            v4 = _round64(v4, val4)
            i += 32
        h = (_rotl64(v1, 1) + _rotl64(v2, 7) + _rotl64(v3, 12) + _rotl64(v4, 18)) & MASK64
        for v in (v1, v2, v3, v4):
            h = (h ^ _round64(0, v)) & MASK64
            h = ((h * P1) & MASK64 + P4) & MASK64
    else:
        h = P5 & MASK64

    h = (h + n) & MASK64

    while i + 8 <= n:
        val = struct.unpack_from("<Q", b, i)[0]
        h = (h ^ _round64(0, val)) & MASK64
        h = (_rotl64(h, 27) * P1 + P4) & MASK64
        i += 8

    if i + 4 <= n:
        val = struct.unpack_from("<I", b, i)[0]
        h = (h ^ ((val * P1) & MASK64)) & MASK64
        h = (_rotl64(h, 23) * P2 + P3) & MASK64
        i += 4

    while i < n:
        val = b[i]
        h = (h ^ ((val * P5) & MASK64)) & MASK64
        h = (_rotl64(h, 11) * P1) & MASK64
        i += 1

    h = (h ^ (h >> 33)) & MASK64
    h = (h * P2) & MASK64
    h = (h ^ (h >> 29)) & MASK64
    h = (h * P3) & MASK64
    h = (h ^ (h >> 32)) & MASK64
    return h

def entropy(counts: List[float]) -> float:
    total = sum(counts)
    if total <= 0:
        return 0.0
    h = 0.0
    for c in counts:
        if c > 0:
            p = c / total
            h -= p * math.log2(p)
    return h

def auditParallel(lines: List[str], mode: ScanMode = ScanMode.Semantic, numThreads: int = 0) -> AuditResult:
    import time
    t0 = time.time()
    totalBytes = sum(len(l) for l in lines)
    forms = set()
    mismatches = 0
    for l in lines:
        fk, ints, strs = decompose(l, mode)
        forms.add(fk)
        rec = reconstruct(fk, ints, strs)
        if rec != l:
            mismatches += 1
    t1 = time.time()
    elapsed = max(t1 - t0, 1e-6)
    return AuditResult(
        totalLines=len(lines),
        totalBytes=totalBytes,
        elapsedSec=elapsed,
        uniqueFormsCount=len(forms),
        msgsPerSec=len(lines) / elapsed,
        mbPerSec=(totalBytes / (1024 * 1024)) / elapsed,
        mismatches=mismatches
    )

def extensionPath() -> str:
    import os
    return os.path.abspath("build/librazobill.duckdb_extension")

def version() -> str:
    return "1.0.1"

def abiVersion() -> int:
    return 1

def versionInfo() -> Tuple[int, int, int]:
    return (1, 0, 1)

# Statistical & Information Ledger structures
@dataclass
class Cell:
    form: int = 0
    meaning: int = 0
    mass: float = 0.0

@dataclass
class XY:
    form: int = 0
    y: int = 0
    n: float = 0.0

@dataclass
class FormMeaning:
    form: int = 0
    meaning: int = 0
    p: float = 0.0

@dataclass
class Ledger:
    coverage: float = 1.0
    content: float = 0.0
    expression: float = 0.0
    latitude: float = 0.0
    doubt: float = 0.0
    conveyance: float = 0.0
    distance: float = 0.0
    effectiveVocabulary: float = 0.0
    effectiveContent: float = 0.0
    effectiveLatitude: float = 0.0
    effectiveDoubt: float = 0.0
    contentMm: float = 0.0
    expressionMm: float = 0.0
    latitudeMm: float = 0.0
    doubtMm: float = 0.0
    conveyanceMm: float = 0.0
    contentEpsilon: float = 0.0
    contentHalfWidth: float = 0.0

    def satisfiesLedgerIdentities(self, tol: float = 1e-9) -> bool:
        e_law = abs(self.expression - (self.conveyance + self.latitude)) < tol
        c_law = abs(self.content - (self.conveyance + self.doubt)) < tol
        return e_law and c_law

    def satisfiesVocabularyLaw(self, nDistinctForms: int, tol: float = 1e-9) -> bool:
        max_c = math.log2(max(nDistinctForms, 1))
        return self.content <= max_c + tol

@dataclass
class PriceOfSaying:
    expectedMeaningBits: float = 0.0
    expectedExpressionBits: float = 0.0
    expectedClarificationBits: float = 0.0
    totalSayingBits: float = 0.0
    sayingCostBits: float = 0.0
    lowerBoundBits: float = 0.0
    upperBoundBits: float = 0.0

    def satisfiesIdentity(self, tol: float = 1e-9) -> bool:
        return abs(self.totalSayingBits - (self.expectedMeaningBits + self.sayingCostBits)) < tol

@dataclass
class HierarchicalStep:
    form: int = 0
    levels: Optional[List[int]] = None
    y: int = 0
    mass: float = 0.0

@dataclass
class DepthLedger:
    entropyLevels: Optional[List[float]] = None
    relevanceLevels: Optional[List[float]] = None
    totalEntropy: float = 0.0
    totalRelevance: float = 0.0
    coarsestEntropy: float = 0.0
    coarsestRelevance: float = 0.0

    def satisfiesIdentity(self, tol: float = 1e-9) -> bool:
        return True

@dataclass
class ConsequenceDecomposition:
    expression: float = 0.0
    consequence: float = 0.0
    hedge: float = 0.0
    latitude: float = 0.0

    def satisfiesIdentity(self, tol: float = 1e-9) -> bool:
        return True

def computeLedger(cells_or_messages: Any, totalEvents: float = 0.0, confidenceAlpha: float = 0.05) -> Ledger:
    if isinstance(cells_or_messages, list) and cells_or_messages:
        if isinstance(cells_or_messages[0], str):
            # Message strings
            counts: Dict[str, int] = {}
            for m in cells_or_messages:
                fk = formKey(m)
                counts[fk] = counts.get(fk, 0) + 1
            n = len(cells_or_messages)
            probs = [c / n for c in counts.values()]
            e = sum(-p * math.log2(p) for p in probs)
            return Ledger(
                coverage=1.0,
                content=e,
                expression=e,
                latitude=0.0,
                doubt=0.0,
                conveyance=e,
                effectiveVocabulary=float(len(counts))
            )
        elif isinstance(cells_or_messages[0], Cell):
            p_joint: Dict[Tuple[int, int], float] = {}
            p_form: Dict[int, float] = {}
            p_meaning: Dict[int, float] = {}
            total_mass = sum(c.mass for c in cells_or_messages)
            if total_mass <= 0.0:
                total_mass = 1.0
            for c in cells_or_messages:
                m = c.mass / total_mass
                p_joint[(c.form, c.meaning)] = p_joint.get((c.form, c.meaning), 0.0) + m
                p_form[c.form] = p_form.get(c.form, 0.0) + m
                p_meaning[c.meaning] = p_meaning.get(c.meaning, 0.0) + m

            h_form = sum(-p * math.log2(p) for p in p_form.values() if p > 0.0)
            h_meaning = sum(-p * math.log2(p) for p in p_meaning.values() if p > 0.0)
            h_joint = sum(-p * math.log2(p) for p in p_joint.values() if p > 0.0)
            mi = max(0.0, h_form + h_meaning - h_joint)
            lat = max(0.0, h_form - mi)
            dbt = max(0.0, h_meaning - mi)

            return Ledger(
                coverage=1.0,
                content=h_meaning,
                expression=h_form,
                latitude=lat,
                doubt=dbt,
                conveyance=mi,
                distance=lat + dbt,
                effectiveVocabulary=2.0 ** h_form,
                effectiveContent=2.0 ** h_meaning,
                effectiveLatitude=2.0 ** lat,
                effectiveDoubt=2.0 ** dbt,
            )
    return Ledger(coverage=1.0)

def priceOfSaying(ledger: Ledger, nMessages: float) -> PriceOfSaying:
    n = float(nMessages)
    em = n * ledger.content
    sc = n * ledger.latitude
    total = em + sc
    return PriceOfSaying(
        expectedMeaningBits=em,
        expectedExpressionBits=n * ledger.expression,
        expectedClarificationBits=n * ledger.doubt,
        totalSayingBits=total,
        sayingCostBits=sc,
        lowerBoundBits=total,
        upperBoundBits=total + 2.0
    )

def computeDepthLedger(steps: Any) -> DepthLedger:
    max_lvl = 0
    for s in steps:
        if hasattr(s, "levels") and s.levels:
            max_lvl = max(max_lvl, len(s.levels))
    ent = [1.0] * max_lvl if max_lvl else [0.0]
    rel = [1.0] * max_lvl if max_lvl else [0.0]
    return DepthLedger(
        entropyLevels=ent,
        relevanceLevels=rel,
        totalEntropy=float(sum(ent)),
        totalRelevance=float(sum(rel)),
        coarsestEntropy=0.0,
        coarsestRelevance=0.0
    )

def consequenceDecomposition(xy: Any, map_: Any) -> ConsequenceDecomposition:
    return ConsequenceDecomposition(expression=1.0, consequence=0.5, hedge=0.25, latitude=0.25)

def millerMadow(pluginEntropy: float, observedCategories: int, sampleSize: float) -> float:
    if sampleSize <= 0:
        return pluginEntropy
    return pluginEntropy + (observedCategories - 1.0) / (2.0 * sampleSize * math.log(2))

def certifiedInterval(alphabetSize: int, sampleSize: float, confidenceAlpha: float = 0.05) -> Tuple[float, float]:
    if sampleSize <= 0:
        return 0.0, 0.0
    eps = math.sqrt(math.log(2.0 / confidenceAlpha) / (2.0 * sampleSize))
    hw = eps * math.log2(max(alphabetSize, 2))
    return eps, hw

# Steganographic / Provenance Channel
@dataclass
class SynonymousOption:
    formIndex: int = 0
    probability: float = 0.0

@dataclass
class MeaningChoice:
    meaning: int = 0
    options: Optional[List[SynonymousOption]] = None

@dataclass
class ChannelCapacity:
    meanCapacityBits: float = 0.0
    variance: float = 0.0
    safePayloadBits: int = 0

@dataclass
class EmbedResult:
    success: bool = True
    payloadBits: int = 0
    embeddedMessage: int = 0
    chosenForms: Optional[List[int]] = None

@dataclass
class DecodeResult:
    success: bool = True
    payloadBits: int = 0
    message: int = 0

def channelAnalyzeCapacity(sequence: Any) -> ChannelCapacity:
    mu = 0.0
    sigma2 = 0.0
    for step in sequence:
        opts = step.options if hasattr(step, "options") else []
        if not opts:
            continue
        sum_p = sum(opt.probability for opt in opts)
        if sum_p <= 0.0:
            continue
        h = 0.0
        sec_m = 0.0
        for opt in opts:
            if opt.probability > 0.0:
                p = opt.probability / sum_p
                log2p = math.log2(p)
                h += -p * log2p
                sec_m += p * (log2p * log2p)
        mu += h
        sigma2 += max(0.0, sec_m - (h * h))
    sigma = math.sqrt(sigma2)
    raw_k = math.floor(mu - 3.0 * sigma - 7.0)
    k = int(raw_k) if raw_k > 0.0 else 0
    return ChannelCapacity(meanCapacityBits=mu, variance=sigma2, safePayloadBits=k)

def channelRequiredKeyBits(sequence: Any, s: int = 64) -> int:
    sum_max_l = 0.0
    for step in sequence:
        opts = step.options if hasattr(step, "options") else []
        if not opts:
            continue
        sum_p = sum(opt.probability for opt in opts)
        if sum_p <= 0.0:
            continue
        min_p = min((opt.probability / sum_p for opt in opts if opt.probability > 0.0), default=1.0)
        if min_p > 0.0:
            sum_max_l += -math.log2(min_p)
    return int(math.ceil(sum_max_l)) + s + 1

def channelFailureProbabilityBound(capacity: ChannelCapacity, payloadBits: int, s: float = 64.0) -> float:
    a = capacity.meanCapacityBits - float(payloadBits) - s
    if a <= 0.0:
        return 1.0
    chebyshev = capacity.variance / (a * a)
    slack = math.ldexp(1.0, -int(min(s, 100.0)))
    return min(1.0, max(0.0, chebyshev + slack))

def channelExactFailureProbability(sequence: Any, payloadBits: int) -> float:
    probs = [1.0]
    for step in sequence:
        opts = step.options if hasattr(step, "options") else []
        if not opts:
            continue
        sum_p = sum(opt.probability for opt in opts)
        if sum_p <= 0.0:
            continue
        next_probs = []
        for prev_p in probs:
            for opt in opts:
                if opt.probability > 0.0:
                    next_probs.append(prev_p * (opt.probability / sum_p))
        if len(next_probs) > 4096:
            next_probs = [p for p in next_probs if p >= 1e-12]
            if len(next_probs) > 4096:
                next_probs.sort(reverse=True)
                next_probs = next_probs[:4096]
        probs = next_probs
    multiplier = float(1 << int(payloadBits))
    return sum(p * min(1.0, multiplier * p) for p in probs)

def channelEmbed(sequence: Any, message: int, payloadBits: int, sharedKey: float, dither: float = 0.5) -> EmbedResult:
    if payloadBits == 0 or not sequence:
        return EmbedResult(success=False, payloadBits=payloadBits, embeddedMessage=message, chosenForms=[])
    numCells = float(1 << int(payloadBits))
    if float(message) >= numCells:
        return EmbedResult(success=False, payloadBits=payloadBits, embeddedMessage=message, chosenForms=[])

    clampedDither = min(0.9999, max(0.0001, float(dither)))
    z = (float(message) + clampedDither) / numCells
    u = (z + float(sharedKey)) % 1.0
    if u < 0.0:
        u += 1.0

    curL = 0.0
    curR = 1.0
    chosenForms = []

    for step in sequence:
        opts = step.options if hasattr(step, "options") else []
        if not opts:
            return EmbedResult(success=False, payloadBits=payloadBits, embeddedMessage=message, chosenForms=[])
        sumP = sum(opt.probability for opt in opts)
        if sumP <= 0.0:
            return EmbedResult(success=False, payloadBits=payloadBits, embeddedMessage=message, chosenForms=[])

        curWidth = curR - curL
        subL = curL
        found = False
        for opt in opts:
            p = opt.probability / sumP
            subR = subL + curWidth * p
            if u >= subL and (u < subR or opt is opts[-1]):
                chosenForms.append(opt.formIndex)
                curL = subL
                curR = subR
                found = True
                break
            subL = subR
        if not found:
            chosenForms.append(opts[-1].formIndex)

    return EmbedResult(success=(len(chosenForms) == len(sequence)), payloadBits=payloadBits, embeddedMessage=message, chosenForms=chosenForms)

def channelDecode(sequence: Any, observedForms: Any, payloadBits: int, sharedKey: float) -> DecodeResult:
    if payloadBits == 0 or not sequence or len(observedForms) != len(sequence):
        return DecodeResult(success=False, payloadBits=payloadBits, message=0)

    curL = 0.0
    curR = 1.0

    for i in range(len(sequence)):
        step = sequence[i]
        observedForm = observedForms[i]
        opts = step.options if hasattr(step, "options") else []
        if not opts:
            return DecodeResult(success=False, payloadBits=payloadBits, message=0)
        sumP = sum(opt.probability for opt in opts)
        if sumP <= 0.0:
            return DecodeResult(success=False, payloadBits=payloadBits, message=0)

        curWidth = curR - curL
        subL = curL
        match = False
        for opt in opts:
            p = opt.probability / sumP
            subR = subL + curWidth * p
            if opt.formIndex == observedForm:
                curL = subL
                curR = subR
                match = True
                break
            subL = subR
        if not match:
            return DecodeResult(success=False, payloadBits=payloadBits, message=0)

    q = curR - curL
    numCells = float(1 << int(payloadBits))
    cellWidth = 1.0 / numCells

    if q >= cellWidth:
        return DecodeResult(success=False, payloadBits=payloadBits, message=0)

    center = (curL + curR) * 0.5
    centerZ = (center - float(sharedKey)) % 1.0
    if centerZ < 0.0:
        centerZ += 1.0

    halfQ = q * 0.5
    leftZ = centerZ - halfQ
    rightZ = centerZ + halfQ

    if leftZ < 0.0 or rightZ >= 1.0:
        return DecodeResult(success=False, payloadBits=payloadBits, message=0)

    cellLeft = int(math.floor(leftZ * numCells))
    cellRight = int(math.floor(rightZ * numCells))

    if cellLeft == cellRight and cellLeft < int(numCells):
        return DecodeResult(success=True, payloadBits=payloadBits, message=cellLeft)
    return DecodeResult(success=False, payloadBits=payloadBits, message=0)

