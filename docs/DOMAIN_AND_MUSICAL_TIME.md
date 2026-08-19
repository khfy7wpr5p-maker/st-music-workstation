# ST Music Workstation — Domain & Musical-Time Contract v0.1

Status: Stage 0-C.1 contract baseline; production implementation and runtime evidence remain deferred

## 1. Scope

Stage 0-C.1 defines the authoritative Musical Time foundation required by the Stage 0-A architecture contract.

This document is documentation-only. It does not add production C++ code, build configuration, dependencies, audio/MIDI engines, notation parsing, plugin hosting, AI behavior, CI, or Stage 0-C.2 domain mappings.

Stage 0-C.1 establishes only:

- the authoritative representation of musical position and duration;
- tempo and meter maps;
- deterministic conversion from musical position to playback/sample position;
- validation and range rules for Musical Time values;
- ownership rules preventing MIDI, MusicXML, Score, TAB, or renderers from becoming independent timing authorities.

## 2. Musical Time ownership invariant

ST Music Workstation owns one authoritative Musical Time model.

Audio, MIDI, Score, Guitar TAB, transport, playback cursor, tempo, and meter must resolve through this shared model.

The following are prohibited as independent authoritative clocks:

- MIDI PPQ/tick values;
- MusicXML divisions;
- notation-renderer coordinates or internal timing units;
- Guitar TAB-local timing counters;
- UI pixel positions;
- plugin-SDK timing structures;
- AI-generated timestamps.

External timing representations may be imported, exported, or adapted, but they must be converted at an explicit boundary into ST-owned Musical Time values.

## 3. Authoritative musical value

The canonical musical value is an exact rational quantity expressed in quarter-note units.

Conceptually:

```text
MusicalValue = numerator / denominator quarter notes
```

Rules:

1. `numerator` is an integer.
2. `denominator` is a strictly positive integer.
3. Values are normalized to a canonical reduced form.
4. Authoritative Musical Time must not depend on binary floating-point equality.
5. Import adapters must preserve exact source timing where representable and must report unsupported or invalid timing rather than silently approximating it.
6. Validity must not depend on native integer width, `size_t`, compiler extensions, or other platform-specific representation capacity. Before production Musical Time types are implemented, a follow-on reviewed contract/build baseline must define a single ST-owned acceptance envelope for numerator/denominator magnitude and intermediate exact-rational arithmetic that is identical on every supported platform.
7. An implementation may use a wider internal representation than the ST acceptance envelope, but it must not accept authoritative values on one supported platform that another conforming supported platform is required to reject solely because of native representation width.
8. Until the platform-independent acceptance envelope is fixed, this Stage 0-C.1 document authorizes the semantic contract only, not a production storage type or platform-specific numeric shortcut.

Examples:

```text
quarter note        = 1
half note           = 2
eighth note         = 1/2
sixteenth note      = 1/4
dotted quarter      = 3/2
quarter-note triplet = 2/3
```

This representation is independent of MIDI PPQ and MusicXML `divisions`.

## 4. MusicalPosition

`MusicalPosition` represents an absolute musical location relative to the project musical origin.

Conceptually:

```text
MusicalPosition = non-negative MusicalValue
```

Rules:

- the project musical origin is position `0`;
- negative authoritative musical positions are invalid in Stage 0-C.1;
- count-in, pre-roll, latency compensation, or other pre-origin behavior must not be modeled by silently introducing negative authoritative positions;
- position equality is exact rational equality after normalization.

## 5. MusicalDuration

`MusicalDuration` represents a musical span using the same exact rational unit as `MusicalPosition`.

Rules:

- duration must be non-negative;
- zero duration is permitted only for domain concepts explicitly defined as point-like events;
- entities that consume time must require duration greater than zero;
- negative duration is invalid;
- duration arithmetic must detect overflow or representation limits rather than wrap silently.

Stage 0-C.1 does not define which Score, MIDI, TAB, Audio, Clip, or Project entities consume these values; those mappings belong to Stage 0-C.2.

## 6. TempoBpm

Tempo is represented by an ST-owned validated exact rational `TempoBpm` value measured in quarter-note beats per minute.

Conceptually:

```text
TempoBpm = numerator / denominator quarter-note beats per minute
```

Contract range:

```text
1 <= TempoBpm <= 1000
```

Rules:

- `numerator` is a strictly positive integer;
- `denominator` is a strictly positive integer;
- the value is normalized to a canonical reduced form;
- NaN is invalid at every adapter or input boundary;
- positive or negative Infinity is invalid at every adapter or input boundary;
- zero and negative BPM are invalid;
- values outside the contract range are invalid;
- an exact decimal source such as `120.5` must be converted from its decimal lexical value to the equivalent exact rational value (`241/2`), not through platform-dependent binary floating-point equality;
- a binary floating-point value received from an external API is boundary data only and must be validated and converted to the canonical rational representation before it can become authoritative state;
- when a source exposes timing only as binary floating point, its adapter contract must define one deterministic source-semantic conversion to an exact rational value (for example, a source-defined decimal interchange value or an exact sign/significand/exponent interpretation). Epsilon comparison, tolerance-based snapping, locale-dependent formatting, platform-default decimal formatting, or undocumented rounding must not decide the authoritative rational value; if the source semantics cannot be established deterministically, the adapter must reject the value rather than guess;
- authoritative persistence stores the canonical rational value, not a platform-specific binary floating-point encoding;
- an adapter must reject invalid tempo values instead of clamping them silently.

The range is intentionally broad enough for normal and extreme musical use while excluding nonsensical or unsafe values.

## 7. TempoMap

`TempoMap` defines tempo changes over authoritative Musical Time.

Each tempo change contains:

```text
TempoChange
  position : MusicalPosition
  tempo    : TempoBpm
```

Invariants:

1. A tempo event must exist at musical position `0`.
2. Tempo-change positions are ordered by exact `MusicalPosition`.
3. At most one authoritative tempo value may exist at the same position.
4. Duplicate/conflicting events at one position are invalid unless an explicit edit operation replaces the prior event before validation.
5. Between two tempo changes, tempo is constant for Stage 0-C.1.
6. A tempo change is effective from its exact `position` inclusive until the next tempo-change position; conceptually tempo segments are half-open intervals `[position_i, position_i+1)`, with the final segment extending forward for as long as project time is defined.
7. Tempo ramps/curves are not part of Stage 0-C.1 and must not be inferred implicitly.

A future contract may add explicit tempo ramps without changing the ownership model defined here.

## 8. Meter

Meter is represented by an ST-owned validated time-signature value:

```text
Meter
  numerator   : integer
  denominator : integer
```

Contract ranges:

```text
1 <= numerator <= 64

denominator ∈ {1, 2, 4, 8, 16, 32, 64}
```

Canonical coordinate values are derived exactly as:

```text
canonicalBeatUnit = 4 / denominator quarter notes
measureLength     = numerator × canonicalBeatUnit
```

Rules:

- zero or negative values are invalid;
- denominator values outside the allowed set are invalid;
- meter values must not be inferred from visual barlines alone;
- the canonical beat used for measure/beat coordinates is always one denominator-note unit;
- compound or additive pulse groupings are derived presentation/grouping metadata and must not replace the canonical beat unit or create another timing authority.

Examples:

```text
4/4 canonical beat unit = quarter note; measure length = 4 quarter notes
3/4 canonical beat unit = quarter note; measure length = 3 quarter notes
6/8 canonical beat unit = eighth note;   measure length = 3 quarter notes
```

A UI may later group 6/8 as two dotted-quarter pulses, but the canonical coordinate system remains beats 1 through 6 in eighth-note units. A separate pulse/grouping view must be explicitly distinguished from canonical `Measure / Beat / Subdivision` coordinates.

## 9. MeterMap

`MeterMap` defines meter changes over authoritative Musical Time.

Each meter change contains:

```text
MeterChange
  position : MusicalPosition
  meter    : Meter
```

Invariants:

1. A meter event must exist at musical position `0`.
2. Meter-change positions are ordered by exact `MusicalPosition`.
3. At most one authoritative meter value may exist at the same position.
4. Conflicting meter events at one position are invalid unless resolved by an explicit edit operation.
5. Measure and beat labels are derived from `MeterMap` plus `MusicalPosition`; they are not independent clocks.
6. Every `MeterChange` position is a derived measure boundary. Position `0` begins the first measure; every later meter change ends the preceding measure at that exact `MusicalPosition` and begins a new measure using the new meter.
7. If a meter change occurs before the measure length implied by the previous meter is complete, the preceding measure is a valid partial measure for coordinate derivation; the change must not be reinterpreted as continuing the previous measure under the new meter.

Stage 0-C.1 does not require a meter change to coincide with a measure boundary that would have been implied by the previous meter. The meter-change position itself establishes the new boundary deterministically. Import and editing layers may later apply stricter musical-policy validation where appropriate.

## 10. Measure and beat coordinates

User-facing coordinates such as measure and beat are derived views over Musical Time.

Conceptually:

```text
MusicalPosition
      +
   MeterMap
      ↓
Measure / Beat / Subdivision view
```

For a position inside a derived measure, let `offset` be the exact MusicalValue from that measure's start and let `canonicalBeatUnit` be defined by the active meter:

```text
beatIndex       = floor(offset / canonicalBeatUnit) + 1
beatSubdivision = offset mod canonicalBeatUnit
```

Rules:

- measure number is not the authoritative time value;
- beat number is not the authoritative time value;
- beat numbering is one-based and uses the active meter's denominator-note unit;
- each meter-change position is beat 1 / the start of the new derived measure;
- compound-meter pulse groupings do not change canonical beat numbering;
- changing meter must not rewrite the underlying identity of a musical position;
- UI measure/beat labels must be reproducible from validated Musical Time and MeterMap state;
- imported measure numbers may be preserved as metadata where needed, but they must not replace the authoritative position model.

## 11. SampleFrame and SampleRate

`SampleFrame` represents an absolute discrete playback frame in an audio-rendering context.

Conceptually:

```text
SampleFrame = non-negative integer
```

The canonical conversion input `SampleRate` is an exact positive rational quantity measured in frames per second:

```text
SampleRate = numerator / denominator frames per second
```

Rules:

- negative sample frames are invalid as authoritative playback positions in Stage 0-C.1;
- `SampleRate.numerator` and `SampleRate.denominator` are strictly positive integers and the value is stored in reduced form;
- NaN, Infinity, zero, or negative sample-rate input is invalid at an adapter boundary;
- a device/API sample-rate value must be validated and converted to the canonical exact rational form before musical-to-playback conversion;
- binary-floating sample-rate inputs are subject to the same deterministic source-semantic conversion rule defined for `TempoBpm`; tolerance-based snapping or undocumented rounding must not create authoritative SampleRate values;
- sample rate is not itself a Musical Time value;
- SampleFrame is a playback representation, not the authoritative musical location;
- Audio, MIDI scheduling, Score cursor, and TAB cursor must derive their playback alignment from the same Musical Time conversion path.

Concrete device/sample-rate support belongs to later Audio and real-time stages.

## 12. Deterministic musical-to-playback conversion

Conversion from `MusicalPosition` to playback time is defined by integrating the validated `TempoMap` from musical origin to the target position.

For a constant-tempo segment:

```text
segmentSeconds = quarterNotes × 60 / TempoBpm
segmentFrames  = quarterNotes × 60 × SampleRate / TempoBpm
```

`quarterNotes`, `TempoBpm`, and `SampleRate` are exact rational values under this contract. Therefore each segment duration and frame count is calculated as an exact rational quantity.

For multiple tempo segments, the total playback duration/frame position is the ordered exact-rational sum of all completed segment contributions plus the target segment contribution.

The conversion pipeline is:

```text
MusicalPosition
      +
   TempoMap
      ↓
Exact rational playback duration / frame position
      +
   SampleRate
      ↓
Round once at final boundary
      ↓
SampleFrame
```

Invariants:

1. Tempo segments must be processed in deterministic position order.
2. Invalid TempoMap state makes conversion fail; it must not produce a best-effort result.
3. All authoritative intermediate tempo, duration, sample-rate, and frame-position calculations use normalized exact rational arithmetic; binary floating-point accumulation is not the contract algorithm.
4. Segment contributions must not be individually rounded to SampleFrame. They are summed as exact rationals first.
5. Conversion to discrete `SampleFrame` rounds exactly once, after the complete rational frame position for the requested `MusicalPosition` has been calculated.
6. Final `SampleFrame` conversion uses round-to-nearest with ties-to-even applied to that exact rational value.
7. The same normalized MusicalPosition, TempoMap, and SampleRate must therefore produce the same SampleFrame result on every conforming platform.
8. Arithmetic representation limits, overflow, underflow, or out-of-range conversion must be reported as failure rather than wrapped, saturated, or approximated silently.

Reverse conversion from `SampleFrame` to `MusicalPosition` treats the discrete frame as the exact rational playback time `SampleFrame / SampleRate`, locates the corresponding TempoMap segment deterministically, and solves that segment using the same exact rational tempo arithmetic. If the resulting musical position cannot be represented within the platform-independent ST acceptance envelope, conversion fails explicitly; reverse conversion must not create a second timing authority.

## 13. Change semantics

Editing tempo or meter changes derived views and playback mappings but does not create a new timing model.

Rules:

- changing tempo changes musical-position ↔ playback-position mapping;
- changing meter changes measure/beat labeling and bar interpretation;
- changing meter does not by itself alter the underlying `MusicalPosition` of existing events;
- changing tempo does not by itself alter the underlying `MusicalPosition` of existing events;
- application commands must validate a proposed map change before it becomes authoritative state.

Undo/redo, command IDs, project revisioning, and persistence transactions are outside Stage 0-C.1.

## 14. Validation and failure semantics

Musical Time validation is deterministic and independent of UI, AI, external notation renderers, and plugin SDKs.

Validation must reject at minimum:

- negative `MusicalPosition`;
- negative `MusicalDuration`;
- zero duration where a consuming entity later requires positive duration;
- rational values with zero/negative denominator;
- arithmetic overflow or unsupported representation size;
- NaN or Infinity in tempo or conversion inputs;
- BPM outside `1..1000`;
- meter numerator outside `1..64`;
- unsupported meter denominator;
- missing tempo at position `0`;
- missing meter at position `0`;
- conflicting tempo events at one position;
- conflicting meter events at one position;
- non-positive, non-finite, or non-representable sample rate supplied to conversion;
- playback/sample-frame conversion overflow.

Validation failure must be explicit. Invalid values must not be silently clamped, guessed, normalized into a different musical meaning, or accepted because an external library accepts them.

## 15. Import/export boundary rules

External formats are adapters to Musical Time, not sources of architectural truth.

### MIDI

MIDI PPQ/ticks must be converted to exact ST MusicalValue quantities using the imported file's validated timing definition. Raw MIDI tick values must not become the Project's authoritative timing type. If a MIDI timing mode (including SMPTE-based timing) cannot be mapped to authoritative Musical Time without unstated tempo/musical assumptions, the adapter must reject it or require an explicit reviewed conversion policy; it must not invent a tempo or silently create a second timing authority.

### MusicXML

MusicXML `divisions`, note durations, measure positions, and related timing fields must be converted into ST Musical Time after import validation. MusicXML divisions must not become a global ST timing grid.

### Notation renderers

Renderer timing, layout coordinates, SVG positions, or renderer-specific event IDs must not become authoritative Musical Time.

### Plugins and devices

Host/device timing structures may be converted at adapter boundaries but must not leak into core Musical Time contracts.

## 16. Prohibited shortcuts

The following implementations violate Stage 0-C.1:

- choosing a fixed MIDI PPQ such as 480/960 as the authoritative project clock;
- using MusicXML `divisions` as the permanent project timing unit;
- maintaining separate Score, MIDI, and TAB clocks;
- storing authoritative musical positions, tempo values, or conversion accumulators as unconstrained `float`/`double` values and relying on raw binary floating-point equality/accumulation for the contract result;
- deriving authoritative validity from platform-native integer width or silently using different numeric acceptance limits on different supported platforms;
- using epsilon/tolerance-based snapping or undocumented binary-float formatting to create authoritative TempoBpm or SampleRate values;
- rounding each tempo segment to SampleFrame before summing the complete target position;
- accepting NaN/Infinity tempo or timing values;
- silently clamping invalid BPM/meter values;
- treating compound-meter pulse groupings as the canonical beat clock;
- allowing notation-renderer or plugin-SDK types to define core timing identity;
- rounding musical durations merely to fit an external format without reporting loss;
- using UI pixels or waveform coordinates as project time.

## 17. Stage 0-C.1 non-goals

Stage 0-C.1 does not define:

- Project, Track, Clip, ScoreNote, MIDI event, TAB event, or AudioClip identity schemas;
- cross-domain event mappings;
- chord/tie/voice/fingering relationships;
- project persistence format beyond the canonical value requirements stated for Musical Time;
- undo/redo transactions;
- audio callback implementation;
- device buffer sizes or supported sample-rate matrix;
- MIDI scheduling implementation;
- notation parsing/rendering implementation;
- MusicXML security schema;
- tempo ramps or automation curves;
- plugin-host timing implementation;
- compound/additive pulse-grouping presentation metadata;
- exact platform-independent numerator/denominator/intermediate-arithmetic magnitude ceilings; these must be fixed by a reviewed follow-on contract/build baseline before production Musical Time types are implemented;
- AI models, inference, or candidate semantics;
- production source code, dependencies, build files, tests, workflows, or CI.

These items require later contracts or implementation stages. Cross-domain identities and mappings are reserved for Stage 0-C.2.

## 18. Stage 0-C.1 acceptance criteria

Stage 0-C.1 contract baseline is acceptable when:

- exactly one ST-owned authoritative Musical Time model is defined;
- musical position and duration use exact rational musical quantities independent of MIDI PPQ and MusicXML divisions;
- TempoBpm and SampleRate have canonical exact rational representations for deterministic conversion;
- binary-floating boundary values cannot become authoritative through platform-dependent formatting, epsilon/tolerance snapping, or undocumented rounding;
- production implementation remains gated on a single platform-independent ST rational/intermediate-arithmetic acceptance envelope;
- tempo and meter validation/ranges are explicit;
- tempo and meter maps have deterministic ordering, origin requirements, and tempo-change interval semantics;
- canonical beat coordinates use the active meter denominator-note unit, with compound/additive pulse grouping kept as a separate derived presentation concern;
- measure/beat coordinates are derived views rather than independent clocks;
- musical-position ↔ playback/sample-position conversion uses exact rational accumulation and a single explicit round-to-nearest, ties-to-even SampleFrame boundary;
- NaN, Infinity, invalid ranges, conflicts, representation limits, and overflow fail explicitly;
- external timing formats remain adapter concerns and unsupported/ambiguous timing modes are not guessed;
- Stage 0-C.2 domain identities/mappings are not prematurely defined;
- no production code, dependency, build, workflow, or CI change is introduced by this stage.
