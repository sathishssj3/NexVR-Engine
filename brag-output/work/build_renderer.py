import base64
import os

FONT_DIR = r"nexvr-client\launcher\public\fonts"
LOGO_PATH = r"nexvr-client\launcher\src\assets\logo.png"

def read_b64(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")

chakra_700_b64 = read_b64(os.path.join(FONT_DIR, "chakra-petch-700.woff2"))
chakra_600_b64 = read_b64(os.path.join(FONT_DIR, "chakra-petch-600.woff2"))
chakra_400_b64 = read_b64(os.path.join(FONT_DIR, "chakra-petch-400.woff2"))
mono_600_b64 = read_b64(os.path.join(FONT_DIR, "jetbrains-mono-600.woff2"))
mono_400_b64 = read_b64(os.path.join(FONT_DIR, "jetbrains-mono-400.woff2"))
inter_600_b64 = read_b64(os.path.join(FONT_DIR, "inter-600.woff2"))
inter_400_b64 = read_b64(os.path.join(FONT_DIR, "inter-400.woff2"))
logo_b64 = read_b64(LOGO_PATH)

html_template = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<style>
@font-face {{
  font-family: 'Chakra Petch';
  font-weight: 700;
  src: url('data:font/woff2;base64,{chakra_700_b64}') format('woff2');
}}
@font-face {{
  font-family: 'Chakra Petch';
  font-weight: 600;
  src: url('data:font/woff2;base64,{chakra_600_b64}') format('woff2');
}}
@font-face {{
  font-family: 'Chakra Petch';
  font-weight: 400;
  src: url('data:font/woff2;base64,{chakra_400_b64}') format('woff2');
}}
@font-face {{
  font-family: 'JetBrains Mono';
  font-weight: 600;
  src: url('data:font/woff2;base64,{mono_600_b64}') format('woff2');
}}
@font-face {{
  font-family: 'JetBrains Mono';
  font-weight: 400;
  src: url('data:font/woff2;base64,{mono_400_b64}') format('woff2');
}}
@font-face {{
  font-family: 'Inter';
  font-weight: 600;
  src: url('data:font/woff2;base64,{inter_600_b64}') format('woff2');
}}
@font-face {{
  font-family: 'Inter';
  font-weight: 400;
  src: url('data:font/woff2;base64,{inter_400_b64}') format('woff2');
}}

* {{
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}}

body {{
  width: 1920px;
  height: 1080px;
  overflow: hidden;
  background-color: #000000;
  color: #FFFFFF;
  font-family: 'Inter', sans-serif;
  user-select: none;
  position: relative;
}}

#viewport {{
  position: absolute;
  top: 0;
  left: 0;
  width: 1920px;
  height: 1080px;
  overflow: hidden;
}}

/* Canvas for background cybernetic grid and particles */
#bgCanvas {{
  position: absolute;
  top: 0;
  left: 0;
  width: 1920px;
  height: 1080px;
  z-index: 1;
}}

/* Top tech header bar */
#hudHeader {{
  position: absolute;
  top: 32px;
  left: 64px;
  right: 64px;
  height: 48px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid rgba(204, 0, 0, 0.3);
  padding-bottom: 12px;
  z-index: 10;
  font-family: 'JetBrains Mono', monospace;
  font-size: 14px;
  letter-spacing: 2px;
}}

.hud-tag {{
  color: #CC0000;
  font-weight: 600;
}}

.hud-val {{
  color: #8E8F9A;
}}

/* Bottom status telemetry */
#hudFooter {{
  position: absolute;
  bottom: 32px;
  left: 64px;
  right: 64px;
  height: 40px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-top: 1px solid rgba(44, 45, 53, 0.8);
  padding-top: 12px;
  z-index: 10;
  font-family: 'JetBrains Mono', monospace;
  font-size: 13px;
  color: #8E8F9A;
  letter-spacing: 1px;
}}

.pulse-dot {{
  display: inline-block;
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #30D158;
  margin-right: 8px;
  box-shadow: 0 0 10px #30D158;
}}

/* Vignette & scanlines */
#vignette {{
  position: absolute;
  top: 0;
  left: 0;
  width: 1920px;
  height: 1080px;
  background: radial-gradient(circle at center, transparent 40%, rgba(0, 0, 0, 0.85) 100%);
  pointer-events: none;
  z-index: 20;
}}

#scanlines {{
  position: absolute;
  top: 0;
  left: 0;
  width: 1920px;
  height: 1080px;
  background: repeating-linear-gradient(0deg, rgba(0,0,0,0.15) 0px, rgba(0,0,0,0.15) 1px, transparent 1px, transparent 3px);
  pointer-events: none;
  z-index: 21;
  opacity: 0.6;
}}

/* Scene Containers */
.scene {{
  position: absolute;
  top: 0;
  left: 0;
  width: 1920px;
  height: 1080px;
  z-index: 5;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  opacity: 0;
  pointer-events: none;
}}

/* Scene 1: The Hook */
#scene1 {{
  text-align: center;
}}

.hook-badge {{
  display: inline-flex;
  align-items: center;
  padding: 8px 20px;
  background: rgba(204, 0, 0, 0.12);
  border: 1px solid rgba(204, 0, 0, 0.6);
  border-radius: 4px;
  font-family: 'JetBrains Mono', monospace;
  font-size: 15px;
  color: #FF3B30;
  letter-spacing: 3px;
  margin-bottom: 28px;
  text-transform: uppercase;
}}

.hook-title {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 88px;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -1px;
  text-transform: uppercase;
  color: #FFFFFF;
  text-shadow: 0 0 35px rgba(204, 0, 0, 0.7);
  margin-bottom: 24px;
}}

.hook-subtitle {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 34px;
  font-weight: 600;
  color: #E0E0E6;
  letter-spacing: 4px;
  text-transform: uppercase;
}}

/* Scene 2: Core Reveal */
#scene2 {{
  display: flex;
  flex-direction: column;
  align-items: center;
}}

.logo-container {{
  position: relative;
  width: 180px;
  height: 180px;
  margin-bottom: 24px;
}}

.logo-glow {{
  position: absolute;
  top: -20px;
  left: -20px;
  right: -20px;
  bottom: -20px;
  background: radial-gradient(circle, rgba(204,0,0,0.6) 0%, transparent 70%);
  border-radius: 50%;
  filter: blur(20px);
}}

.logo-img {{
  position: relative;
  width: 100%;
  height: 100%;
  object-fit: contain;
  filter: drop-shadow(0 0 25px rgba(204, 0, 0, 0.8));
}}

.product-title {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 82px;
  font-weight: 700;
  letter-spacing: 4px;
  text-transform: uppercase;
  color: #FFFFFF;
  margin-bottom: 12px;
  text-shadow: 0 0 30px rgba(204, 0, 0, 0.6);
}}

.product-tagline {{
  font-family: 'JetBrains Mono', monospace;
  font-size: 20px;
  color: #CC0000;
  letter-spacing: 5px;
  text-transform: uppercase;
  margin-bottom: 40px;
}}

.api-grid {{
  display: flex;
  gap: 24px;
}}

.api-card {{
  width: 220px;
  padding: 18px 20px;
  background: #080808;
  border: 1px solid #2C2D35;
  border-radius: 6px;
  text-align: center;
  position: relative;
  overflow: hidden;
  box-shadow: 0 10px 30px rgba(0,0,0,0.8);
}}

.api-card::before {{
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 2px;
  background: linear-gradient(90deg, transparent, #CC0000, transparent);
}}

.api-name {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 20px;
  font-weight: 700;
  color: #FFFFFF;
  margin-bottom: 6px;
}}

.api-desc {{
  font-family: 'JetBrains Mono', monospace;
  font-size: 11px;
  color: #8E8F9A;
  letter-spacing: 1px;
}}

/* Scene 3: Stereoscopic Reprojection View */
#scene3 {{
  display: flex;
  flex-direction: row;
  justify-content: center;
  align-items: center;
  gap: 60px;
  padding: 0 100px;
}}

.stereo-display {{
  display: flex;
  flex-direction: column;
  gap: 16px;
  width: 900px;
}}

.stereo-views {{
  display: flex;
  gap: 24px;
}}

.eye-viewport {{
  flex: 1;
  height: 380px;
  background: #050505;
  border: 1px solid #2C2D35;
  border-radius: 8px;
  position: relative;
  overflow: hidden;
  box-shadow: 0 12px 40px rgba(0,0,0,0.9);
}}

.eye-canvas {{
  width: 100%;
  height: 100%;
}}

.eye-label {{
  position: absolute;
  top: 14px;
  left: 16px;
  padding: 4px 10px;
  background: rgba(0,0,0,0.7);
  border: 1px solid #CC0000;
  border-radius: 3px;
  font-family: 'JetBrains Mono', monospace;
  font-size: 11px;
  color: #FFFFFF;
  letter-spacing: 1.5px;
}}

.eye-fov {{
  position: absolute;
  bottom: 14px;
  right: 16px;
  font-family: 'JetBrains Mono', monospace;
  font-size: 11px;
  color: #30D158;
}}

.specs-column {{
  width: 600px;
  display: flex;
  flex-direction: column;
  gap: 20px;
}}

.section-headline {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 46px;
  font-weight: 700;
  color: #FFFFFF;
  letter-spacing: 2px;
  text-transform: uppercase;
  margin-bottom: 6px;
}}

.section-accent {{
  color: #CC0000;
}}

.spec-item {{
  background: #080808;
  border: 1px solid #2C2D35;
  border-left: 3px solid #CC0000;
  padding: 16px 20px;
  border-radius: 4px;
}}

.spec-title {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 20px;
  font-weight: 700;
  color: #FFFFFF;
  margin-bottom: 4px;
}}

.spec-desc {{
  font-family: 'JetBrains Mono', monospace;
  font-size: 13px;
  color: #A0A1AC;
  line-height: 1.4;
}}

/* Scene 4: Launcher UI Simulation */
#scene4 {{
  display: flex;
  flex-direction: column;
  align-items: center;
}}

.launcher-window {{
  width: 1200px;
  background: #050505;
  border: 1px solid #2C2D35;
  border-radius: 8px;
  overflow: hidden;
  box-shadow: 0 20px 60px rgba(0,0,0,0.95), 0 0 40px rgba(204,0,0,0.15);
}}

.launcher-titlebar {{
  height: 48px;
  background: #080808;
  border-bottom: 1px solid #2C2D35;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 24px;
  font-family: 'JetBrains Mono', monospace;
  font-size: 13px;
}}

.launcher-content {{
  padding: 32px;
  display: flex;
  gap: 32px;
}}

.game-list {{
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 12px;
}}

.game-item {{
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 20px;
  background: #0A0A0A;
  border: 1px solid #2C2D35;
  border-radius: 6px;
}}

.game-item.active {{
  border-color: #CC0000;
  background: rgba(204, 0, 0, 0.08);
}}

.game-info-title {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 18px;
  font-weight: 600;
  color: #FFFFFF;
}}

.game-info-badge {{
  font-family: 'JetBrains Mono', monospace;
  font-size: 11px;
  color: #8E8F9A;
  margin-top: 2px;
}}

.inject-panel {{
  width: 360px;
  background: #0A0A0A;
  border: 1px solid #2C2D35;
  border-radius: 6px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
}}

.inject-btn {{
  width: 100%;
  height: 52px;
  background: #CC0000;
  border: none;
  border-radius: 4px;
  font-family: 'Chakra Petch', sans-serif;
  font-size: 18px;
  font-weight: 700;
  letter-spacing: 2px;
  color: #FFFFFF;
  text-transform: uppercase;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 12px;
  box-shadow: 0 0 25px rgba(204, 0, 0, 0.5);
}}

.inject-btn.injected {{
  background: #30D158;
  box-shadow: 0 0 30px rgba(48, 209, 88, 0.6);
}}

.mock-cursor {{
  position: absolute;
  width: 24px;
  height: 24px;
  pointer-events: none;
  z-index: 100;
}}

/* Scene 5: Outro & Punchline */
#scene5 {{
  text-align: center;
}}

.outro-punchline {{
  font-family: 'Chakra Petch', sans-serif;
  font-size: 64px;
  font-weight: 700;
  letter-spacing: 3px;
  text-transform: uppercase;
  color: #FFFFFF;
  margin-bottom: 20px;
  text-shadow: 0 0 40px rgba(204, 0, 0, 0.8);
}}

.outro-version {{
  display: inline-block;
  padding: 8px 24px;
  background: rgba(48, 209, 88, 0.15);
  border: 1px solid #30D158;
  border-radius: 20px;
  font-family: 'JetBrains Mono', monospace;
  font-size: 16px;
  color: #30D158;
  letter-spacing: 2px;
  margin-bottom: 36px;
}}

.outro-links {{
  font-family: 'JetBrains Mono', monospace;
  font-size: 24px;
  color: #FFFFFF;
  letter-spacing: 3px;
}}

.outro-url {{
  color: #FF3B30;
  font-weight: 600;
}}

</style>
</head>
<body>

<div id="viewport">
  <canvas id="bgCanvas" width="1920" height="1080"></canvas>
  <div id="vignette"></div>
  <div id="scanlines"></div>

  <!-- Persistent Tech HUD Header -->
  <div id="hudHeader">
    <div>
      <span class="hud-tag">RUNTIME:</span> <span class="hud-val">NEXVR NATIVE CORE v0.1.97</span>
      <span style="margin: 0 16px; color: #2C2D35;">|</span>
      <span class="hud-tag">TARGET:</span> <span class="hud-val">OPENXR 1.0.34 (VDXR / STEAMVR)</span>
    </div>
    <div>
      <span class="hud-tag">PIPELINE:</span> <span class="hud-val">DIRECT COMPUTE 6DOF</span>
      <span style="margin: 0 16px; color: #2C2D35;">|</span>
      <span class="hud-tag">HEADSET:</span> <span class="hud-val" id="headsetTrackingVal">TRACKING LOCKED [6-DOF]</span>
    </div>
  </div>

  <!-- Persistent Tech HUD Footer -->
  <div id="hudFooter">
    <div>
      <span class="pulse-dot"></span>
      <span style="color: #FFFFFF; font-weight: 600;">ACTIVE SESSION</span>
      <span style="margin: 0 16px; color: #2C2D35;">|</span>
      <span>COMPUTE DISPATCH: 100% HARDWARE ACCELERATED</span>
    </div>
    <div>
      <span>DISPARITY: 64.0mm</span>
      <span style="margin: 0 16px; color: #2C2D35;">|</span>
      <span style="color: #30D158; font-weight: 600;">90.0 FPS [11.1ms]</span>
    </div>
  </div>

  <!-- SCENE 1: THE HOOK (0.0s - 3.2s) -->
  <div class="scene" id="scene1">
    <div class="hook-badge">DIRECT COMPUTE PC VR REVOLUTION</div>
    <div class="hook-title">FLATSCREEN GAMING<br><span style="color: #CC0000;">IS DEAD.</span></div>
    <div class="hook-subtitle">STEP INSIDE YOUR PC GAMES IN FULL 6DOF VR</div>
  </div>

  <!-- SCENE 2: CORE REVEAL (3.2s - 7.5s) -->
  <div class="scene" id="scene2">
    <div class="logo-container">
      <div class="logo-glow"></div>
      <img class="logo-img" src="data:image/png;base64,{logo_b64}" alt="NexVR Logo">
    </div>
    <div class="product-title">NEXVR ENGINE</div>
    <div class="product-tagline">UNIVERSAL HIGH-PERFORMANCE 6DOF VR RUNTIME</div>
    <div class="api-grid">
      <div class="api-card">
        <div class="api-name">DIRECTX 11</div>
        <div class="api-desc">FORMAT NORMALIZED 10-BIT TO 8-BIT COMPUTE</div>
      </div>
      <div class="api-card">
        <div class="api-name">DIRECTX 12</div>
        <div class="api-desc">DIRECT OPENXR SWAPCHAIN UAV DISPATCH</div>
      </div>
      <div class="api-card">
        <div class="api-name">VULKAN</div>
        <div class="api-desc">SPIR-V ASYNC SHADER DISPATCH PIPELINE</div>
      </div>
      <div class="api-card" style="border-color: #CC0000;">
        <div class="api-name" style="color: #FF3B30;">OPENXR</div>
        <div class="api-desc">META QUEST 3 • VDXR • STEAMVR NATIVE</div>
      </div>
    </div>
  </div>

  <!-- SCENE 3: REAL-TIME REPROJECTION (7.5s - 12.0s) -->
  <div class="scene" id="scene3">
    <div class="stereo-display">
      <div class="stereo-views">
        <div class="eye-viewport">
          <canvas id="leftEyeCanvas" class="eye-canvas" width="440" height="380"></canvas>
          <div class="eye-label">LEFT EYE VIEW (OPENXR)</div>
          <div class="eye-fov">FOV: 104° × 104°</div>
        </div>
        <div class="eye-viewport">
          <canvas id="rightEyeCanvas" class="eye-canvas" width="440" height="380"></canvas>
          <div class="eye-label">RIGHT EYE VIEW (OPENXR)</div>
          <div class="eye-fov">DISPARITY: +32.0mm</div>
        </div>
      </div>
      <div style="display: flex; justify-content: space-between; font-family: 'JetBrains Mono', monospace; font-size: 13px; color: #8E8F9A; padding: 4px 10px;">
        <span>CAMERA PROJECTION: REVERSE-Z DYNAMIC</span>
        <span style="color: #30D158;">REPROJECTION TIME: 0.85ms</span>
      </div>
    </div>

    <div class="specs-column">
      <div class="section-headline">REAL-TIME <span class="section-accent">COMPUTE</span> REPROJECTION</div>
      <div class="spec-item">
        <div class="spec-title">SUB-1.5ms GPU OVERHEAD</div>
        <div class="spec-desc">Runs entirely in parallel on asynchronous compute queues. Zero stutter, locked 90 FPS OpenXR delivery.</div>
      </div>
      <div class="spec-item">
        <div class="spec-title">ZERO GAME MODDING REQUIRED</div>
        <div class="spec-desc">Hooks natively at the DXGI/Vulkan swapchain. No game files modified, no anti-cheat triggers.</div>
      </div>
      <div class="spec-item">
        <div class="spec-title">TRUE 6DOF HEAD TRACKING</div>
        <div class="spec-desc">Extracts camera matrices and depth buffer automatically with heuristic pattern matching.</div>
      </div>
    </div>
  </div>

  <!-- SCENE 4: ONE-CLICK INJECTION (12.0s - 16.5s) -->
  <div class="scene" id="scene4">
    <div class="launcher-window">
      <div class="launcher-titlebar">
        <div style="display: flex; align-items: center; gap: 10px;">
          <span style="width: 10px; height: 10px; border-radius: 50%; background: #CC0000; display: inline-block;"></span>
          <span style="color: #FFFFFF; font-weight: 600;">NEXVR LAUNCHER</span>
          <span style="color: #8E8F9A;">• PROFILE MANAGER</span>
        </div>
        <div>
          <span style="color: #30D158;">CONNECTED TO VDXR RUNTIME</span>
        </div>
      </div>
      <div class="launcher-content">
        <div class="game-list">
          <div class="game-item active" id="gameItemSekiro">
            <div>
              <div class="game-info-title">Sekiro: Shadows Die Twice</div>
              <div class="game-info-badge">DirectX 11 • 4K HDR Normalization • Reverse-Z</div>
            </div>
            <div style="font-family: 'JetBrains Mono'; font-size: 13px; color: #30D158;">PROFILE VERIFIED</div>
          </div>
          <div class="game-item">
            <div>
              <div class="game-info-title">Hogwarts Legacy</div>
              <div class="game-info-badge">DirectX 12 • Native Stereo Compute UAV • 90 FPS</div>
            </div>
            <div style="font-family: 'JetBrains Mono'; font-size: 13px; color: #8E8F9A;">READY</div>
          </div>
          <div class="game-item">
            <div>
              <div class="game-info-title">Cyberpunk 2077</div>
              <div class="game-info-badge">DirectX 12 • Real-Time AI Depth Inpainting</div>
            </div>
            <div style="font-family: 'JetBrains Mono'; font-size: 13px; color: #8E8F9A;">READY</div>
          </div>
        </div>

        <div class="inject-panel">
          <div>
            <div style="font-family: 'Chakra Petch'; font-size: 22px; font-weight: 700; color: #FFFFFF; margin-bottom: 8px;">INSTANT INJECTION</div>
            <div style="font-family: 'JetBrains Mono'; font-size: 12px; color: #8E8F9A; line-height: 1.5; margin-bottom: 20px;">
              Zero-configuration runtime hook. Launch target title and NexVR Engine automatically links the OpenXR swapchain.
            </div>
            <div style="background: #000000; border: 1px solid #2C2D35; border-radius: 4px; padding: 12px; margin-bottom: 20px;">
              <div style="font-family: 'JetBrains Mono'; font-size: 11px; color: #8E8F9A;">INJECTION STATUS:</div>
              <div id="injectStatusText" style="font-family: 'JetBrains Mono'; font-size: 14px; font-weight: 600; color: #FF3B30; margin-top: 4px;">READY TO INJECT</div>
            </div>
          </div>

          <div id="injectBtn" class="inject-btn">
            <span id="injectBtnText">INJECT ENGINE NOW</span>
          </div>
        </div>
      </div>
    </div>

    <!-- Simulated Mouse Cursor -->
    <svg id="mouseCursor" class="mock-cursor" viewBox="0 0 24 24" fill="none">
      <path d="M4 2L10 21L13.5 14L20.5 12.5L4 2Z" fill="#FFFFFF" stroke="#000000" stroke-width="1.5" stroke-linejoin="round"/>
    </svg>
  </div>

  <!-- SCENE 5: OUTRO & PUNCHLINE (16.5s - 20.0s) -->
  <div class="scene" id="scene5">
    <div class="logo-container" style="width: 140px; height: 140px; margin-bottom: 20px;">
      <div class="logo-glow" style="background: radial-gradient(circle, rgba(204,0,0,0.8) 0%, transparent 70%);"></div>
      <img class="logo-img" src="data:image/png;base64,{logo_b64}" alt="NexVR Logo">
    </div>
    <div class="outro-punchline">DON'T JUST PLAY THE GAME.<br><span style="color: #CC0000;">BE IN IT.</span></div>
    <div class="outro-version">NEXVR ENGINE v0.1.97 IS LIVE</div>
    <div class="outro-links">
      GET IT NOW: <span class="outro-url">nexvr-engine.pages.dev</span>
    </div>
    <div style="font-family: 'JetBrains Mono'; font-size: 14px; color: #8E8F9A; letter-spacing: 2px; margin-top: 14px;">
      OPEN SOURCE • UNIVERSAL 6DOF VR • GITHUB
    </div>
  </div>

</div>

<script>
const bgCanvas = document.getElementById('bgCanvas');
const bgCtx = bgCanvas.getContext('2d');
const leftCanvas = document.getElementById('leftEyeCanvas');
const leftCtx = leftCanvas.getContext('2d');
const rightCanvas = document.getElementById('rightEyeCanvas');
const rightCtx = rightCanvas.getContext('2d');

const scene1 = document.getElementById('scene1');
const scene2 = document.getElementById('scene2');
const scene3 = document.getElementById('scene3');
const scene4 = document.getElementById('scene4');
const scene5 = document.getElementById('scene5');

const mouseCursor = document.getElementById('mouseCursor');
const injectBtn = document.getElementById('injectBtn');
const injectBtnText = document.getElementById('injectBtnText');
const injectStatusText = document.getElementById('injectStatusText');

function smoothstep(edge0, edge1, x) {{
  x = Math.max(0, Math.min(1, (x - edge0) / (edge1 - edge0)));
  return x * x * (3 - 2 * x);
}}

function drawBackground(t) {{
  bgCtx.fillStyle = '#000000';
  bgCtx.fillRect(0, 0, 1920, 1080);

  // Dynamic 3D horizon grid
  const horizonY = 540;
  const gridSpeed = (t * 80) % 60;
  
  bgCtx.strokeStyle = 'rgba(204, 0, 0, 0.18)';
  bgCtx.lineWidth = 1;

  // Perspective floor lines
  for (let x = -960; x <= 2880; x += 120) {{
    bgCtx.beginPath();
    bgCtx.moveTo(960 + (x - 960) * 0.15, horizonY);
    bgCtx.lineTo(x, 1080);
    bgCtx.stroke();
  }}

  // Horizontal floor lines moving forward
  for (let i = 0; i < 15; i++) {{
    let z = (i * 60 + gridSpeed);
    let y = horizonY + Math.pow(z / 900, 2.2) * 540;
    if (y <= 1080) {{
      let alpha = Math.min(0.4, (y - horizonY) / 540);
      bgCtx.strokeStyle = `rgba(204, 0, 0, ${{alpha}})`;
      bgCtx.beginPath();
      bgCtx.moveTo(0, y);
      bgCtx.lineTo(1920, y);
      bgCtx.stroke();
    }}
  }}

  // Floating particles
  for (let i = 0; i < 40; i++) {{
    let seed = i * 137.5;
    let px = (Math.sin(seed) * 960 + 960 + t * (20 + (i % 30))) % 1920;
    let py = (Math.cos(seed) * 540 + 540 + Math.sin(t * 1.5 + i) * 40) % 1080;
    let pSize = 1.5 + (i % 3);
    let pAlpha = 0.2 + 0.3 * Math.sin(t * 3.0 + i);
    bgCtx.fillStyle = `rgba(255, 60, 60, ${{pAlpha}})`;
    bgCtx.fillRect(px, py, pSize, pSize);
  }}

  // Center horizon glow
  let glowGrad = bgCtx.createRadialGradient(960, horizonY, 50, 960, horizonY, 600);
  glowGrad.addColorStop(0, 'rgba(204, 0, 0, 0.25)');
  glowGrad.addColorStop(1, 'transparent');
  bgCtx.fillStyle = glowGrad;
  bgCtx.fillRect(0, 0, 1920, 1080);
}}

function drawStereoScene(ctx, t, eyeOffset) {{
  ctx.fillStyle = '#050505';
  ctx.fillRect(0, 0, 440, 380);

  // 3D wireframe cyber room
  const cx = 220 + eyeOffset;
  const cy = 190;
  
  ctx.strokeStyle = '#2C2D35';
  ctx.lineWidth = 1;

  // Render 3D rotating cubes representing game world with stereo disparity
  const angle = t * 1.2;
  const numObjects = 5;

  for (let i = 0; i < numObjects; i++) {{
    const objAngle = angle + (i * Math.PI * 2 / numObjects);
    const radius = 110;
    const depth = 200 + Math.sin(objAngle) * 70;
    
    // Perspective projection
    const scale = 220 / depth;
    const px = cx + Math.cos(objAngle) * radius * scale;
    const py = cy + Math.sin(t * 2.0 + i) * 30 * scale;
    const size = 35 * scale;

    ctx.strokeStyle = i === 0 ? '#CC0000' : 'rgba(255, 255, 255, 0.4)';
    ctx.lineWidth = i === 0 ? 2 : 1;
    ctx.strokeRect(px - size/2, py - size/2, size, size);

    if (i === 0) {{
      ctx.fillStyle = 'rgba(204, 0, 0, 0.2)';
      ctx.fillRect(px - size/2, py - size/2, size, size);
    }}
  }}

  // Draw 6DOF crosshair tracking reticle
  ctx.strokeStyle = 'rgba(48, 209, 88, 0.8)';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(cx, cy, 18, 0, Math.PI * 2);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(cx - 26, cy); ctx.lineTo(cx - 10, cy);
  ctx.moveTo(cx + 10, cy); ctx.lineTo(cx + 26, cy);
  ctx.moveTo(cx, cy - 26); ctx.lineTo(cx, cy - 10);
  ctx.moveTo(cx, cy + 10); ctx.lineTo(cx, cy + 26);
  ctx.stroke();
}}

window.setTimestamp = function(t) {{
  // 1. Render Background
  drawBackground(t);

  // 2. Control Scenes Visibility & Animations
  // S1: 0.0s - 3.2s
  let s1Alpha = 0;
  if (t < 3.2) {{
    let enter = smoothstep(0.0, 0.6, t);
    let exit = 1.0 - smoothstep(2.8, 3.2, t);
    s1Alpha = enter * exit;
  }}
  scene1.style.opacity = s1Alpha;
  scene1.style.transform = `scale(${{0.95 + s1Alpha * 0.05}})`;

  // S2: 3.2s - 7.5s
  let s2Alpha = 0;
  if (t >= 3.0 && t < 7.5) {{
    let enter = smoothstep(3.2, 3.8, t);
    let exit = 1.0 - smoothstep(7.0, 7.5, t);
    s2Alpha = enter * exit;
  }}
  scene2.style.opacity = s2Alpha;
  scene2.style.transform = `scale(${{0.92 + s2Alpha * 0.08}})`;

  // S3: 7.5s - 12.0s
  let s3Alpha = 0;
  if (t >= 7.2 && t < 12.0) {{
    let enter = smoothstep(7.5, 8.1, t);
    let exit = 1.0 - smoothstep(11.5, 12.0, t);
    s3Alpha = enter * exit;
  }}
  scene3.style.opacity = s3Alpha;
  if (s3Alpha > 0) {{
    drawStereoScene(leftCtx, t, -12);
    drawStereoScene(rightCtx, t, +12);
  }}

  // S4: 12.0s - 16.5s
  let s4Alpha = 0;
  if (t >= 11.7 && t < 16.5) {{
    let enter = smoothstep(12.0, 12.6, t);
    let exit = 1.0 - smoothstep(16.0, 16.5, t);
    s4Alpha = enter * exit;
  }}
  scene4.style.opacity = s4Alpha;

  // S4 Cursor & Click simulation
  if (t >= 12.0 && t < 16.5) {{
    const cursorProgress = smoothstep(12.5, 14.0, t);
    // Cursor starts at bottom right and moves to inject button
    const curX = 1450 - cursorProgress * 180;
    const curY = 750 - cursorProgress * 230;
    mouseCursor.style.left = `${{curX}}px`;
    mouseCursor.style.top = `${{curY}}px`;

    // Click at t = 14.2s
    if (t >= 14.2) {{
      injectBtn.classList.add('injected');
      injectBtnText.innerText = 'HOOKED [90 FPS LOCKED]';
      injectStatusText.innerText = 'STATUS: INJECTED (OPENXR ACTIVE)';
      injectStatusText.style.color = '#30D158';
    }} else {{
      injectBtn.classList.remove('injected');
      injectBtnText.innerText = 'INJECT ENGINE NOW';
      injectStatusText.innerText = 'READY TO INJECT';
      injectStatusText.style.color = '#FF3B30';
    }}
  }}

  // S5: 16.5s - 20.0s
  let s5Alpha = 0;
  if (t >= 16.2) {{
    let enter = smoothstep(16.5, 17.2, t);
    let exit = 1.0 - smoothstep(19.6, 20.0, t);
    s5Alpha = enter * exit;
  }}
  scene5.style.opacity = s5Alpha;
  scene5.style.transform = `scale(${{0.94 + s5Alpha * 0.06}})`;
}};

// Set initial frame 0
window.setTimestamp(0.0);
</script>
</body>
</html>
"""

output_html = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\brag-output\work\renderer.html"
with open(output_html, "w", encoding="utf-8") as f:
    f.write(html_template)

print(f"Successfully generated {output_html} ({len(html_template)} bytes)")
