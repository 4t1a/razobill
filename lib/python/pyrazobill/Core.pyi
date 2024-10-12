from enum import Enum
from typing import List, Sequence, Tuple

class ScanMode(Enum):
    Exact = ...
    DigitOnly = ...
    Semantic = ...

class SlotRecord:
    formKey: str
    formKeyHash: int
    ints: List[int]
    strs: List[str]
    skeleton: str
    types: str
    def reconstruct(self) -> str: ...

class AuditResult:
    totalLines: int
    totalBytes: int
    elapsedSec: float
    uniqueFormsCount: int
    msgsPerSec: float
    mbPerSec: float
    mismatches: int

class Cell:
    form: int
    meaning: int
    mass: float
    def __init__(self, form: int, meaning: int, mass: float) -> None: ...

class XY:
    form: int
    y: int
    n: float
    def __init__(self, form: int, y: int, n: float) -> None: ...

class FormMeaning:
    form: int
    meaning: int
    p: float
    def __init__(self, form: int, meaning: int, p: float) -> None: ...

class Ledger:
    coverage: float
    content: float
    expression: float
    latitude: float
    doubt: float
    conveyance: float
    distance: float
    effectiveVocabulary: float
    effectiveContent: float
    effectiveLatitude: float
    effectiveDoubt: float
    contentMm: float
    expressionMm: float
    latitudeMm: float
    doubtMm: float
    conveyanceMm: float
    contentEpsilon: float
    contentHalfWidth: float
    def satisfiesLedgerIdentities(self, tol: float = 1e-9) -> bool: ...
    def satisfiesVocabularyLaw(
        self, nDistinctForms: int, tol: float = 1e-9
    ) -> bool: ...

class ConsequenceDecomposition:
    expression: float
    consequence: float
    hedge: float
    latitude: float
    def satisfiesIdentity(self, tol: float = 1e-9) -> bool: ...

class PriceOfSaying:
    expectedMeaningBits: float
    expectedExpressionBits: float
    expectedClarificationBits: float
    totalSayingBits: float
    sayingCostBits: float
    lowerBoundBits: float
    upperBoundBits: float
    def satisfiesIdentity(self, tol: float = 1e-9) -> bool: ...

class HierarchicalStep:
    form: int
    levels: List[int]
    y: int
    mass: float
    def __init__(
        self, form: int, levels: Sequence[int], y: int, mass: float
    ) -> None: ...

class DepthLedger:
    entropyLevels: List[float]
    relevanceLevels: List[float]
    totalEntropy: float
    totalRelevance: float
    coarsestEntropy: float
    coarsestRelevance: float
    def satisfiesIdentity(self, tol: float = 1e-9) -> bool: ...

class SynonymousOption:
    formIndex: int
    probability: float
    def __init__(self, formIndex: int, probability: float) -> None: ...

class MeaningChoice:
    meaning: int
    options: List[SynonymousOption]
    def __init__(self, meaning: int, options: List[SynonymousOption]) -> None: ...

class ChannelCapacity:
    meanCapacityBits: float
    variance: float
    safePayloadBits: int

class EmbedResult:
    success: bool
    payloadBits: int
    embeddedMessage: int
    chosenForms: List[int]

class DecodeResult:
    success: bool
    payloadBits: int
    message: int

def parse(msg: str, mode: ScanMode = ScanMode.Semantic) -> SlotRecord: ...
def parseBatch(
    msgs: Sequence[str], mode: ScanMode = ScanMode.Semantic
) -> List[SlotRecord]: ...
def decompose(
    msg: str, mode: ScanMode = ScanMode.Semantic
) -> Tuple[str, List[int], List[str]]: ...
def decomposeBatch(
    msgs: Sequence[str], mode: ScanMode = ScanMode.Semantic
) -> List[Tuple[str, List[int], List[str]]]: ...
def reconstruct(formKey: str, ints: Sequence[int], strs: Sequence[str]) -> str: ...
def formKey(msg: str, mode: ScanMode = ScanMode.Semantic) -> str: ...
def templateOf(form: str) -> str: ...
def hash64(s: str) -> int: ...
def entropy(counts: Sequence[float]) -> float: ...
def auditParallel(
    lines: Sequence[str], mode: ScanMode = ScanMode.Semantic, numThreads: int = 0
) -> AuditResult: ...
def extensionPath() -> str: ...
def version() -> str: ...
def abiVersion() -> int: ...
def versionInfo() -> Tuple[int, int, int]: ...
def computeLedger(
    cells: Sequence[Cell], totalEvents: float, confidenceAlpha: float = 0.05
) -> Ledger: ...
def priceOfSaying(ledger: Ledger, nMessages: float) -> PriceOfSaying: ...
def computeDepthLedger(steps: Sequence[HierarchicalStep]) -> DepthLedger: ...
def consequenceDecomposition(
    xy: Sequence[XY], map: Sequence[FormMeaning]
) -> ConsequenceDecomposition: ...
def millerMadow(
    pluginEntropy: float, observedCategories: int, sampleSize: float
) -> float: ...
def certifiedInterval(
    alphabetSize: int, sampleSize: float, confidenceAlpha: float = 0.05
) -> Tuple[float, float]: ...
def channelAnalyzeCapacity(sequence: Sequence[MeaningChoice]) -> ChannelCapacity: ...
def channelRequiredKeyBits(sequence: Sequence[MeaningChoice], s: int = 64) -> int: ...
def channelFailureProbabilityBound(
    capacity: ChannelCapacity, payloadBits: int, s: float = 64.0
) -> float: ...
def channelExactFailureProbability(
    sequence: Sequence[MeaningChoice], payloadBits: int
) -> float: ...
def channelEmbed(
    sequence: Sequence[MeaningChoice],
    message: int,
    payloadBits: int,
    sharedKey: float,
    dither: float = 0.5,
) -> EmbedResult: ...
def channelDecode(
    sequence: Sequence[MeaningChoice],
    observedForms: Sequence[int],
    payloadBits: int,
    sharedKey: float,
) -> DecodeResult: ...
