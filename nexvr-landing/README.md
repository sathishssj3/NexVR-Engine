# NexVR Engine — Landing Page

A console/headset dashboard for NexVR Engine: void black, ice white and crimson,
Oxanium type, and a live stereoscopic viewport that renders a real 3D room through
two VR lens barrels.

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
| Background | `--void` | `#08090D` |
| Text | `--ice` | `#E8EDF2` |
| Accent / signal | `--red` | `#D90429` |

Every panel, border, glow, and state is one of these three or an alpha variant
(`--line` is ice at 14%, `--red-soft` is red at 12%, and so on). **If a fourth hue
appears, the palette is broken.** A test in the verification pass walks every
rendered element and asserts the set of computed colours never exceeds those three
plus `--void-deep` (`#05060A`, the viewport's inner black).

### Red is rationed

This is the rule that keeps a red brand from looking cheap. Red carries roughly
**15%** of the surface and only ever means one of four things:

1. a primary action (CTA fills, the download button)
2. an active / hover / focus state (nav, chips, tabs, open FAQ rows, focus rings)
3. a live VR status (`tag--on`, the viewport readout values)
4. the portal itself (the gate in the 3D room, and the accent headline words)

Everything structural is ice on void: body copy, headings, borders, panels, the
room wireframe, step numbers, stat figures, pricing amounts, tick marks. Several of
those started red during the rebuild and were deliberately pulled back — four large
red stat figures plus red prices plus red CTAs tipped the page into RGB-gaming
territory and away from premium.

Status is encoded inside the budget: red = tested, ice = partial, dimmed = in
development — each with its own pixel glyph, so meaning never rests on colour alone.

## Typography

- **H1, H2, buttons, technical metadata** — Oxanium (500/600/700)
- **Body** — Inter (400/500/600)
- **Logo** — custom lettering, drawn as SVG paths on a 32-unit grid in
  `#wm-nex` / `#wm-vr`. Not set in any typeface.

## The logo

Rebuilt from the supplied artwork as vector rather than embedded as a raster, so it
stays crisp from a 16px favicon to a full-bleed lockup and takes its colours from
the palette tokens.

- **`#wm-nex` / `#wm-vr`** — the wordmark, heavy geometric letterforms on a 32-unit
  grid. Split in two so each half takes its own fill: NEX in ice, VR in red.
- **`#halftone-fine` / `#halftone-coarse`** — the dot disc, as tiled SVG patterns.
  Two densities because the fine grid turns to mush when shrunk to nav height.
- **`#logo-disc-sm`** — the coarse disc, for the nav lockup.
- The **footer** carries the full lockup: disc behind the wordmark, wordmark
  overhanging it, as in the original.

The wordmark is split across two `<use>` elements rather than styled by descendant
selectors, because CSS cannot match elements inside a `<use>` shadow tree — `fill`
inherits through it from the `<use>` itself. Same reason `.halftone-dot` is styled
on the pattern's own circle, which does live in the main document.

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

- **The room is ice; only the gate is red.** The wireframe is structure, so it takes
  the text colour. The gate is the portal — the one thing in the viewport that earns
  the accent — and it is the only element given a `shadowBlur` bloom, paid on ~16
  segments out of ~130.
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

- `#E8EDF2` on `#08090D` is ~16:1. Dimmed ice is used only for non-essential
  metadata. Red is never used for body copy — only for short labels, numerals and
  fills, where its contrast on void is comfortable.
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
   The logo needs nothing — it is inline SVG.
5. Add an OG image and set `og:image`.
