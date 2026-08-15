# NexVR Engine — Landing Page

A console/headset dashboard for NexVR Engine: phosphor-green CRT palette, Oxanium
type, and a live stereoscopic viewport that renders a real 3D room through two VR
lens barrels.

Zero dependencies, zero build step.

```
nexvr-landing/
├── index.html                       # structure, custom wordmark, SVG defs
├── assets/
│   ├── css/styles.css               # tokens → primitives → sections → responsive
│   ├── js/main.js                   # reveals, skeletons, index, viewport engine
│   ├── js/compatibility-data.js     # PLACEHOLDER game index — replace before launch
│   └── img/favicon.svg
└── README.md
```

## Running it

```bash
cd nexvr-landing
python3 -m http.server 8000
# → http://localhost:8000
```

A server is needed only so the browser will load the sibling asset files.

## Three colours. That's the whole system.

| Role | Token | Hex |
|---|---|---|
| Background | `--void` | `#0A0E0A` |
| Text | `--phosphor` | `#D6F5D6` |
| Accent / signal | `--green` | `#39FF14` |

Every panel, border, glow, and state in the stylesheet is one of these three or an
alpha variant of one of them (`--line` is phosphor at 14%, `--green-soft` is green
at 12%, and so on). **If a fourth hue appears, the palette is broken.** A test in
the verification pass walks every rendered element and asserts that the set of
computed colours never exceeds those three plus `--void-deep` (`#070907`, the
viewport's inner black).

Status is encoded with only those three: green = tested, phosphor = partial,
dimmed = in development — with a distinct pixel glyph on each so the meaning never
rests on colour alone.

## Typography

- **H1, H2, buttons, technical metadata** — Oxanium (500/600/700)
- **Body** — Inter (400/500/600)
- **Logo** — custom lettering, drawn as SVG paths on a 32-unit grid in
  `#wm-nex` / `#wm-vr`. Not set in any typeface.

The wordmark is split into two `<use>` elements rather than one, because CSS
selectors cannot match elements inside a `<use>` shadow tree — `fill` inherits
through it from the `<use>` itself, which is how NEX stays phosphor while VR
takes the accent.

## The viewport

The centrepiece. Not a picture of VR — an actual one.

Each frame:

1. **A 3D room is projected twice**, once per eye, offset by a true 64mm
   interpupillary distance in camera space. Floor and ceiling grids, wall uprights,
   four floating crates, and a bright gate at the far end — roughly 130 line
   segments, hand-built in `buildRoom()`.
2. **Both eyes draw into one wide source buffer**, so the per-frame pixel read is a
   single `getImageData` instead of two.
3. **Each eye is gathered through a lens-barrel LUT** — a standard radial
   polynomial, `r' = r(1 + k₁r² + k₂r⁴)` — precomputed once at load. That is the
   curve you can see in the floor grid.
4. **The discs are blitted** to the visible canvas.

Some details worth knowing if you touch it:

- **Chromatic aberration is a radial two-tap smear, not an RGB channel split.** A
  real channel split invents magenta and cyan fringes, and this page has a hard
  three-colour budget. The smear reads as lens softness and stays in the palette.
- **The rim vignette is baked into the source**, not multiplied per output pixel —
  it costs nothing that way, and its steep falloff is what makes each eye read as a
  round lens rather than one wide letterbox.
- **Line brightness is quantised into six buckets** so the room draws in ~12 stroke
  calls instead of one per segment. Canvas2D spends far more time on draw-call
  overhead than on the lines themselves.
- **`k₂` is capped** so the outermost sample still lands inside the source buffer.
  Raise it and the disc rim goes black.
- **It runs uncapped.** A full stereo frame measures well under a millisecond, so
  there is no fixed fps gate — a gate just beats against the display's cadence and
  makes head tracking feel worse than it is.

**Controls.** Head tracking follows the pointer, with a slow idle drift when the
pointer leaves. Two keyboard-operable buttons sit in the HUD: **Stereo/Flat**
toggles between the headset view and a single flat frame — the injection story in
one click — and **Pause/Play** stops the loop. Under `prefers-reduced-motion` the
loop never starts, Pause reports itself as already stopped rather than claiming to
run, and the viewport paints one static frame that still responds to the pointer.

## Skeleton loading

The panel edge paints instantly so structure exists before data does, then the
content area fills with chunky 16px blocks swept by a `steps()` animation — low-res
on purpose. The compatibility index is rendered from data specifically so this does
real work rather than decorating a static page; swap the `setTimeout` in `main.js`
for a `fetch()` and it keeps working unchanged.

## Responsive

- **Phone** (`≤720px`) — single column, sticky bottom CTA, dimmed background grid.
- **Nav** (`≤900px`) — collapses to the hamburger before the layout does, since it
  runs out of room first.
- **Tablet** (`≤1024px`) — grids collapse, viewport moves above the copy, hover
  swaps to press states via `@media (hover: none)`, 50px touch targets.
- **TV** (`≥1900px`, or `≥1600px` with a coarse pointer) — 8vw safe margins,
  1.375rem base text, 74px buttons, two-column maximum, and 4px focus rings with a
  12px halo for D-pad navigation.

## Accessibility

- `#D6F5D6` on `#0A0E0A` is ~15:1. Dimmed phosphor is used only for non-essential
  metadata.
- Every animation — block shimmer, particles, reveals, the viewport — is disabled or
  reduced under `prefers-reduced-motion`.
- The viewport's controls are real buttons, keyboard-operable; pointer look is an
  extra, never the only way in. The canvas carries a descriptive `aria-label`.
- Skip link, real `<details>` accordions, arrow-key tab navigation, `aria-live` on
  the index while it loads, visible focus everywhere.
- Status never rests on colour alone — each state carries its own pixel glyph.
- A `forced-colors` block drops the chamfers and grid for plain borders.

## Verified

Rendered in Chromium at 375 / 768 / 1024 / 1440 / 1920 plus a reduced-motion pass:
no JS errors, no horizontal overflow at any width, nav fits on one line, all 29
reveals fire, the index populates, the palette audit holds, and the viewport paints.

## Before launch

1. Replace `assets/js/compatibility-data.js` with the verified index, then remove
   the `.game__sample` chip in `gameCard()` and the preview note in the
   Compatibility header. **Every entry in that file is placeholder content, not a
   test result.**
2. Replace the `Fig. 01–04` values in `index.html` (`data-count`) with real numbers.
3. Point the install snippets and the installer button at real URLs.
4. Self-host Oxanium and Inter if you would rather not depend on Google Fonts.
5. Add an OG image and set `og:image`.
