# NexVR Engine — Landing Page

The marketing site for NexVR Engine, built to the **"Gilded Arcade"** direction: an
ancient temple built out of pixels. Carved gold ornament and marble stillness, cut
through with the flicker of an 8-bit portal.

Zero dependencies, zero build step. Three files do the whole job.

```
nexvr-landing/
├── index.html                       # all page structure + the shared SVG ornament defs
├── assets/
│   ├── css/styles.css               # tokens → primitives → sections → responsive
│   ├── js/main.js                   # reveals, skeletons, hall, portal canvas
│   ├── js/compatibility-data.js     # PLACEHOLDER game index — replace before launch
│   └── img/favicon.svg
└── README.md
```

## Running it

Any static server works; the page needs one only so the browser will load the
sibling asset files.

```bash
cd nexvr-landing
python3 -m http.server 8000
# → http://localhost:8000
```

## The style formula

| Layer | Share | Role | Where it lives |
|---|---|---|---|
| Dark neoclassical | 50% | Architecture — layout, proportion, carved serif type, marble ground | `.shell`, `.section`, `.display`, `body::before` vein texture |
| Filigree | 30% | Trim — hairline frames, corner flourishes, etched dividers | `.plaque`, `.fil-c`, `.rule`, `#fil-corner` / `#fil-divider` |
| Pixel art | 20% | Spark — badges, particles, loading, cursor | `.pixel-tag`, `.dust`, `.skeleton`, `.pixel-cursor`, the portal canvas |

The three never compete for the same real estate: filigree frames content but never
fills it, and the pixel layer only appears at label size or in motion.

## Design tokens

Defined once on `:root` in `styles.css`.

| Role | Token | Hex |
|---|---|---|
| Primary background | `--obsidian` | `#0B0A0C` |
| Panels | `--plum` | `#1C1620` |
| Primary accent | `--gold` | `#C6A15B` |
| Accent depth | `--brass` | `#8C6A3F` |
| Primary text | `--bone` | `#EDE6D6` |
| Secondary text | `--violet-grey` | `#6B5F72` |
| Portal accent | `--cyan` | `#4FF3E8` |

Cyan is the only saturated colour in the system and is reserved for VR-portal
moments — particles, focus/active states, the loading shimmer. Type is Cinzel
(display), Inter (body), and Press Start 2P (labels at 9–14px only, never body copy).

## The two things worth knowing about

**The portal (`#portal-canvas`).** A stylised vista is drawn procedurally at canvas
resolution, twice: once as a flat single view, once as a side-by-side stereo pair with
per-eye parallax and lens vignette. The transition between them is a real per-block
dissolve — a stable random threshold field decides when each 10px block flips, and
blocks at the wavefront are drawn offset and tinted cyan, so the flat frame visibly
shatters into VR. It runs at 30fps, only while on screen, only while the tab is
visible, and paints a single static stereo frame under `prefers-reduced-motion`.

Because it animates on its own, it carries two real controls in the HUD — **Inject**
(advance a phase) and **Pause/Play** — both keyboard-operable buttons rather than a
bare click handler on the canvas. Clicking the canvas still works as a pointer
shortcut, but it is never the only way in. While paused, **Inject** jumps straight to
the destination state instead of animating.

**The skeleton system (`.skeleton`).** Not a smooth shimmer. The filigree frame paints
instantly so structure exists before data does, and the content area fills with chunky
16px gold-on-charcoal blocks swept by a `steps()` animation — low-res on purpose.
Larger blocks carry `LOADING…` in the pixel font. The Compatibility Hall is rendered
from data specifically so this system does real work rather than decorating a
static page; swap the `setTimeout` in `main.js` for a `fetch()` and it keeps working
unchanged.

## Responsive behaviour

- **Phone** (`≤720px`) — single column, sigil hamburger, sticky bottom CTA, ornament
  density cut (one corner flourish instead of four, watermark dimmed and unanimated).
- **Tablet** (`≤1024px`) — grids collapse, portal moves above the copy, hover effects
  swap to press states via `@media (hover: none)`, touch targets grow to 52px.
- **Desktop** — full multi-column, hover glow, pixel-dust scatter on hall cards,
  optional pixel cursor (toggle in the footer, remembered in `localStorage`).
- **TV / 10-foot** (`≥1900px`, or `≥1600px` with a coarse pointer) — 8vw safe margins,
  1.375rem base text, 72px buttons, two-column maximum, and 4px focus rings with a
  12px gold halo for D-pad navigation.

## Accessibility

- Body text is `#EDE6D6` on `#0B0A0C` — 15.8:1, well past WCAG AA. Muted violet-grey is
  used only for non-essential metadata at ≥14px.
- Every animation — the pixel shimmer, dust particles, etch-in reveals, and the portal —
  is disabled or reduced under `prefers-reduced-motion`. The portal's Pause control
  reports itself as already stopped in that mode rather than claiming to be running.
- Status badges are pixel-grid SVGs (`#px-check`, `#px-warn`, `#px-gear`), not emoji.
  Emoji render in their own colours and would break the gold-on-black palette at
  exactly the size they're used, and they read unpredictably to screen readers.
- Skip link, real `<details>` accordions, arrow-key tab navigation, `aria-live` on the
  hall while it loads, and visible focus everywhere.
- Ornament is vector SVG and `aria-hidden`; it stays crisp from phone to TV and silent
  to screen readers. A `forced-colors` block swaps ornament for plain borders.
- The pixel font never runs longer than a few words.

## Before launch

1. Replace `assets/js/compatibility-data.js` with the verified index, then remove the
   `.game__sample` chip in `gameCard()` and the preview note in the Compatibility Hall
   header. **Every entry currently in that file is placeholder content, not a test
   result.**
2. Replace the `Fig. 1–4` values in `index.html` (`data-count`) with real numbers.
3. Point the install snippets and the installer button at real URLs.
4. Self-host the three fonts if you would rather not depend on Google Fonts.
5. Add an OG image and set `og:image`.
