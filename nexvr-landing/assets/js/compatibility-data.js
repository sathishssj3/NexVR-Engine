/* ==========================================================================
   Compatibility Hall — PLACEHOLDER DATA
   --------------------------------------------------------------------------
   Every entry below is sample content for design purposes only. None of it
   represents a real test result, and each card renders a visible "SAMPLE"
   tag for that reason. Replace this array wholesale with the verified index
   before launch, and drop the `sample: true` flag once entries are real.

   Schema:
     title   string   game name
     year    number   release year, shown as metadata
     api     string   graphics API the profile hooks
     status  string   "tested" | "partial" | "progress"
     note    string   one line on what the profile does or lacks
     specs   string[] short spec chips shown in the card footer
   ========================================================================== */
window.NEXVR_COMPATIBILITY = [
  {
    title: "Half-Life 2",
    year: 2004,
    api: "DX11 (via dxlevel)",
    status: "tested",
    note: "Full 6DoF with recovered view matrix. HUD reprojected to a comfortable depth.",
    specs: ["6DoF", "HUD DEPTH", "GTX 1060+"]
  },
  {
    title: "Dark Souls III",
    year: 2016,
    api: "DX11",
    status: "tested",
    note: "Stereo pass stable at 90 Hz. Third-person camera anchored with world-scale tuning.",
    specs: ["6DoF", "SNAP TURN", "RTX 2060+"]
  },
  {
    title: "Cyberpunk 2077",
    year: 2020,
    api: "DX12",
    status: "partial",
    note: "Stereo renders correctly; volumetric fog and some screen-space effects render per-eye inconsistently.",
    specs: ["6DoF", "FOG ARTIFACTS", "RTX 4070+"]
  },
  {
    title: "Portal 2",
    year: 2011,
    api: "DX11",
    status: "tested",
    note: "Reference profile. Portal surfaces resolve per eye without ghosting.",
    specs: ["6DoF", "MOTION AIM", "GTX 1060+"]
  },
  {
    title: "Subnautica",
    year: 2018,
    api: "DX11 (Unity)",
    status: "tested",
    note: "Engine-level profile shared across Unity titles of the same generation.",
    specs: ["6DoF", "UNITY PROFILE", "RTX 2060+"]
  },
  {
    title: "Elden Ring",
    year: 2022,
    api: "DX12",
    status: "partial",
    note: "Anti-cheat must be disabled; offline play only. Cutscenes fall back to a flat theater view.",
    specs: ["OFFLINE ONLY", "FLAT CUTSCENES", "RTX 3070+"]
  },
  {
    title: "The Witcher 3",
    year: 2015,
    api: "DX11 / DX12",
    status: "tested",
    note: "DX11 path is the recommended profile; the DX12 path costs roughly 15% more frame time.",
    specs: ["6DoF", "WORLD SCALE", "RTX 3060+"]
  },
  {
    title: "Resident Evil 4 (2023)",
    year: 2023,
    api: "DX12",
    status: "progress",
    note: "Matrix recovery lands; per-eye culling still under work, so stereo cost is currently near 2×.",
    specs: ["IN DEV", "HIGH COST", "RTX 4080+"]
  },
  {
    title: "Deep Rock Galactic",
    year: 2020,
    api: "DX11 (UE4)",
    status: "tested",
    note: "Unreal 4 generic profile. Multiplayer sessions are blocked by the launcher.",
    specs: ["6DoF", "UE4 PROFILE", "GTX 1660+"]
  },
  {
    title: "Outer Wilds",
    year: 2019,
    api: "DX11 (Unity)",
    status: "tested",
    note: "Comfort preset ships with reduced roll and an optional static cockpit reference frame.",
    specs: ["6DoF", "COMFORT MODE", "GTX 1660+"]
  },
  {
    title: "Baldur's Gate 3",
    year: 2023,
    api: "DX11 / Vulkan",
    status: "progress",
    note: "DX11 path in testing. The Vulkan path waits on the layer-based injector.",
    specs: ["IN DEV", "DX11 PATH", "RTX 3070+"]
  },
  {
    title: "Hollow Knight",
    year: 2017,
    api: "DX11 (Unity 2D)",
    status: "partial",
    note: "2D titles get a curved virtual screen rather than true stereo depth.",
    specs: ["VIRTUAL SCREEN", "NO 6DoF", "GTX 1050+"]
  }
];
