# ST Music Workstation — Domain & Musical-Time Contract v0.1

Status: Stage 0-C.1 draft contract

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

Tempo is represented by an ST-owned validated `TempoBpm` value.

Contract range:

```text
1.0 <= TempoBpm <= 1000.0
```

Rules:

- NaN is invalid;
- positive or negative Infinity is invalid;
- zero and negative BPM are invalid;
- values outside the contract range are invalid;
- authoritative persistence must use a deterministic decimal/rational representation rather than rely on platform-specific binary floating-point serialization;
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
6. Tempo ramps/curves are not part of Stage 0-C.1 and must not be inferred implicitly.

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

Rules:

- zero or negative values are invalid;
- denominator values outside the allowed set are invalid;
- meter values must not be inferred from visual barlines alone;
- additive/compound display conventions may be represented later, but their underlying authoritative duration must remain compatible with the shared Musical Time model.

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

Stage 0-C.1 does not require meter changes to occur only at bar boundaries. Import and editing layers may later apply stricter musical-policy validation where appropriate.

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

Rules:

- measure number is not the authoritative time value;
- beat number is not the authoritative time value;
- changing meter must not rewrite the underlying identity of a musical position;
- UI measure/beat labels must be reproducible from validated Musical Time and MeterMap state;
- imported measure numbers may be preserved as metadata where needed, but they must not replace the authoritative position model.

## 11. SampleFrame

`SampleFrame` represents an absolute discrete playback frame in an audio-rendering context.

Conceptually:

```text
SampleFrame = non-negative integer
```

Rules:

- negative sample frames are invalid as authoritative playback positions in Stage 0-C.1;
- conversion requires a strictly positive finite sample rate supplied by the playback context;
- sample rate is not itself a Musical Time value;
- SampleFrame is a playback representation, not the authoritative musical location;
- Audio, MIDI scheduling, Score cursor, and TAB cursor must derive their playback alignment from the same Musical Time conversion path.

Concrete device/sample-rate support belongs to later Audio and real-time stages.

## 12. Deterministic musical-to-playback conversion

Conversion from `MusicalPosition` to playback time is defined by integrating the validated `TempoMap` from musical origin to the target position.

For a constant-tempo segment:

```text
seconds = quarter_notes × 60 / BPM
```

For multiple tempo segments, the total playback duration is the ordered sum of each segment duration.

The conversion pipeline is:

```text
MusicalPosition
      +
   TempoMap
      ↓
Exact/controlled playback duration
      +
   SampleRate
      ↓
SampleFrame
```

Invariants:

1. Tempo segments must be processed in deterministic position order.
2. Invalid TempoMap state makes conversion fail; it must not produce a best-effort result.
3. Intermediate calculations must use deterministic precision sufficient to avoid platform-dependent ordering or cumulative drift.
4. Conversion to discrete `SampleFrame` is the only Stage 0-C.1 boundary that requires integer rounding.
5. `SampleFrame` conversion uses round-to-nearest with ties-to-even.
6. The same input MusicalPosition, TempoMap, and sample rate must produce the same SampleFrame result on all supported platforms within the defined numeric representation.
7. Overflow, underflow, or out-of-range conversion must be reported as failure rather than wrapped or saturated silently.

Reverse conversion from `SampleFrame` to `MusicalPosition` must use the same TempoMap and deterministic segment boundaries. It may return an exact position when representable or a bounded conversion result whose precision contract is made explicit by the implementation stage; it must not create a second timing authority.

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
- BPM outside `1.0..1000.0`;
- meter numerator outside `1..64`;
- unsupported meter denominator;
- missing tempo at position `0`;
- missing meter at position `0`;
- conflicting tempo events at one position;
- conflicting meter events at one position;
- non-positive or non-finite sample rate supplied to conversion;
- playback/sample-frame conversion overflow.

Validation failure must be explicit. Invalid values must not be silently clamped, guessed, normalized into a different musical meaning, or accepted because an external library accepts them.

## 15. Import/export boundary rules

External formats are adapters to Musical Time, not sources of architectural truth.

### MIDI

MIDI PPQ/ticks must be converted to exact ST MusicalValue quantities using the imported file's timing definition. Raw MIDI tick values must not become the Project's authoritative timing type.

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
- storing authoritative musical positions as unconstrained `float`/`double` values and comparing them by raw equality;
- accepting NaN/Infinity tempo or timing values;
- silently clamping invalid BPM/meter values;
- allowing notation-renderer or plugin-SDK types to define core timing identity;
- rounding musical durations merely to fit an external format without reporting loss;
- using UI pixels or waveform coordinates as project time.

## 17. Stage 0-C.1 non-goals

Stage 0-C.1 does not define:

- Project, Track, Clip, ScoreNote, MIDI event, TAB event, or AudioClip identity schemas;
- cross-domain event mappings;
- chord/tie/voice/fingering relationships;
- project persistence format;
- undo/redo transactions;
- audio callback implementation;
- device buffer sizes or supported sample-rate matrix;
- MIDI scheduling implementation;
- notation parsing/rendering implementation;
- MusicXML security schema;
- tempo ramps or automation curves;
- plugin-host timing implementation;
- AI models, inference, or candidate semantics;
- production source code, dependencies, build files, tests, workflows, or CI.

These items require later contracts or implementation stages. Cross-domain identities and mappings are reserved for Stage 0-C.2.

## 18. Stage 0-C.1 acceptance criteria

Stage 0-C.1 is acceptable when:

- exactly one ST-owned authoritative Musical Time model is defined;
- musical position and duration use exact rational musical quantities independent of MIDI PPQ and MusicXML divisions;
- tempo and meter validation/ranges are explicit;
- tempo and meter maps have deterministic ordering and origin requirements;
- measure/beat coordinates are derived views rather than independent clocks;
- musical-position ↔ playback/sample-position conversion is deterministic and has an explicit rounding boundary;
- NaN, Infinity, invalid ranges, conflicts, and overflow fail explicitly;
- external timing formats remain adapter concerns;
- Stage 0-C.2 domain identities/mappings are not prematurely defined;
- no production code, dependency, build, workflow, or CI change is introduced by this stage.
