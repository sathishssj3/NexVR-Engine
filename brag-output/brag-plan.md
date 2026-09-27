# NexVR Engine — Launch Video Plan (brag-slim)

## 1. Creative Strategy
- **Product:** NexVR Engine (Universal 6DOF Stereoscopic VR Runtime for PC Games)
- **Target Audience:** PC VR enthusiasts with Meta Quest (Virtual Desktop/Link), SteamVR, or Bigscreen Beyond who want to play AAA flatscreen games in true VR.
- **Angle:** Zero game modification. Native C++ injection into DX11, DX12, and Vulkan swapchains with sub-1.5ms direct compute reprojection.
- **Tone:** `cinematic` / `polished` — High-tech, dark void, tactical neon crimson, cybernetic instrumentation.
- **Visual Identity:**
  - Void Black (`#000000`), Dark Panel (`#050505`), Border (`#2C2D35`), Neon Cyber Red (`#CC0000` / `#FF2A2A`), Status Green (`#30D158`).
  - Typography: `Chakra Petch` (Display & Titles), `JetBrains Mono` (Telemetry & Code), `Inter` (UI).
- **Target Duration:** 20.0s (600 frames @ 30 FPS).
- **Resolution:** 1920 × 1080 (16:9 Landscape).

---

## 2. Scene-by-Scene Storyboard

| Scene | Time / Frames | Visual Action | Headline / Copy | Sound Beat |
|---|---|---|---|---|
| **Scene 1: The Hook** | 0.0s – 3.2s<br>(f0 – f95) | Dark void, glowing perspective depth grid emerges with glowing crimson pulse. | **FLATSCREEN IS DEAD.**<br>STEP INSIDE YOUR PC GAMES. | Sub-bass drop + high-tech riser |
| **Scene 2: Core Reveal** | 3.2s – 7.5s<br>(f96 – f225) | Wireframe targeting reticle locks in; NexVR logo resolves with dynamic neon red edge glow. | **NEXVR ENGINE**<br>`UNIVERSAL 6DOF VR RUNTIME`<br>DX11 • DX12 • VULKAN • OPENXR | 130 BPM cyberpunk bassline & hi-hat groove kicks in |
| **Scene 3: Real-Time Reprojection** | 7.5s – 12.0s<br>(f226 – f360) | Interactive split stereoscopic eye disparity view (Left/Right Eye) with real-time 6DOF motion vector grid. | **REAL-TIME GPU REPROJECTION**<br>• Sub-1.5ms Compute Latency<br>• Zero Game Modding Required<br>• Locked 90 FPS OpenXR Stream | Full synth chords + sweeping lowpass filter |
| **Scene 4: Instant Injection & Compatibility** | 12.0s – 16.5s<br>(f361 – f495) | NexVR Launcher interface with game cards (Sekiro, Hogwarts Legacy, Cyberpunk). Simulated click on `INJECT NOW` turns status neon emerald. | **ONE-CLICK INJECTION**<br>`STATUS: HOOKED [90 FPS]`<br>Virtual Desktop • Quest 3 • SteamVR | Driving arpeggios + snare riser |
| **Scene 5: Punchline & Outro** | 16.5s – 20.0s<br>(f496 – f599) | Cinematic letterbox snap. Official NexVR emblem, release badge `v0.1.97 NOW LIVE`. | **DON'T PLAY THE GAME. BE IN IT.**<br>nexvr-engine.pages.dev | Final cinematic boom + decaying sub-bass tail |

---

## 3. Technical Delivery Pipeline
1. **Audio Synthesis:** Generate custom 20.0s 44.1kHz stereo WAV track in Python with punchy synth bass, cyberpunk beat, risers, and impact hits.
2. **Visual Renderer:** Pixel-perfect HTML5 canvas & DOM renderer in Playwright Chromium, rendering all 600 frames at 1920×1080.
3. **Encoding:** ffmpeg encode with `libx264` (yuv420p, high profile, CRF 18) + AAC audio.
4. **Poster & Frame 0:** Extract peak settled frame (f180 / f540) to `brag.jpg` and bake into frame 0.
5. **Share Copy:** `share-copy.txt` for Reddit/Twitter/Discord release announcement.
