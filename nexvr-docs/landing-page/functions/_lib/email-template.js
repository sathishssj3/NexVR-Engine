/**
 * Release Email Template Builder for Stereo Engine
 * Shared across Admin UI API and CLI Broadcast script.
 */

export const DEFAULT_CONFIG = {
  appName: 'Stereo Engine',
  companyName: 'Dimension 9',
  version: 'v0.1.3',
  subject: 'Stereo Engine v0.1.3 — Early Access Build',
  downloadUrl: 'https://nexvr-engine.pages.dev/api/dl',
  websiteUrl: 'https://nexvr-engine.pages.dev/',
};

export function buildEmailHtml(recipientEmail, customVersion = null) {
  const version = customVersion || DEFAULT_CONFIG.version;
  const appName = DEFAULT_CONFIG.appName;
  const companyName = DEFAULT_CONFIG.companyName;
  const downloadUrl = DEFAULT_CONFIG.downloadUrl;
  const websiteUrl = DEFAULT_CONFIG.websiteUrl;

  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${appName} is Available</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600;700&family=Inter:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    @keyframes emailCardIn {
      from { opacity: 0; transform: translateY(16px); }
      to { opacity: 1; transform: translateY(0); }
    }
    @keyframes laserSweep {
      0% { background-position: -200% 0; }
      100% { background-position: 200% 0; }
    }
    @keyframes radarPing {
      0%, 100% {
        transform: scale(1);
        opacity: 1;
      }
      50% {
        transform: scale(1.25);
        opacity: 0.65;
      }
    }

    .email-container {
      animation: emailCardIn 0.7s cubic-bezier(0.16, 1, 0.3, 1) forwards;
      transition: box-shadow 0.35s ease, border-color 0.35s ease;
    }
    .email-container:hover {
      box-shadow: 0 14px 40px rgba(0, 0, 0, 0.7);
      border-color: rgba(255, 255, 255, 0.12) !important;
    }

    .laser-accent {
      background: linear-gradient(90deg, #FF2A1F, #FF554A, #FF2A1F, #8E0F09, #FF2A1F) !important;
      background-size: 200% 100% !important;
      animation: laserSweep 5s linear infinite;
    }

    .header-brand {
      transition: letter-spacing 0.3s cubic-bezier(0.16, 1, 0.3, 1), color 0.3s ease;
      cursor: default;
    }
    .header-brand:hover {
      letter-spacing: 0.28em !important;
      color: #FFFFFF !important;
    }

    .hero-title-red {
      color: #FF2A1F;
      display: inline-block;
      transition: color 0.25s ease, transform 0.25s ease;
    }
    .hero-title-red:hover {
      color: #FF4238;
      transform: translateY(-1px);
    }

    @keyframes sheenSweep {
      0% {
        transform: translateX(-150%) skewX(-20deg);
        opacity: 0;
      }
      10% {
        opacity: 1;
      }
      25% {
        transform: translateX(260%) skewX(-20deg);
        opacity: 1;
      }
      26%, 100% {
        transform: translateX(260%) skewX(-20deg);
        opacity: 0;
      }
    }

    @keyframes arrowBounce {
      0%, 100% {
        transform: translateY(0);
      }
      50% {
        transform: translateY(3px);
      }
    }

    @keyframes btnBorderPulse {
      0%, 100% {
        border-color: #FF2A1F;
        box-shadow: 0 3px 10px rgba(0, 0, 0, 0.45);
      }
      50% {
        border-color: #FF665E;
        box-shadow: 0 4px 14px rgba(0, 0, 0, 0.55), 0 0 10px rgba(255, 42, 31, 0.22);
      }
    }

    .btn-cta {
      animation: btnBorderPulse 3s infinite ease-in-out;
      transition: transform 0.22s cubic-bezier(0.2, 1, 0.3, 1), box-shadow 0.22s cubic-bezier(0.2, 1, 0.3, 1), background-color 0.2s ease, border-color 0.2s ease !important;
      will-change: transform;
    }
    .btn-cta:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.6), 0 0 14px rgba(255, 42, 31, 0.3) !important;
      background-color: #FF3D33 !important;
      border-color: #FFA39E !important;
    }
    .btn-cta:active {
      transform: translateY(0) scale(0.97);
      transition-duration: 0.08s;
    }

    .btn-link {
      position: relative;
      overflow: hidden;
      display: inline-block;
      text-decoration: none;
      transition: letter-spacing 0.25s cubic-bezier(0.2, 1, 0.3, 1), color 0.2s ease;
    }
    .btn-link::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      width: 50%;
      height: 100%;
      background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.35), transparent);
      transform: translateX(-150%) skewX(-20deg);
      animation: sheenSweep 3.6s infinite cubic-bezier(0.4, 0, 0.2, 1);
      pointer-events: none;
    }
    .btn-cta:hover .btn-link {
      letter-spacing: 0.18em !important;
    }
    .btn-arrow {
      display: inline-block;
      margin-left: 6px;
      font-size: 14px;
      line-height: 1;
      animation: arrowBounce 1.6s infinite ease-in-out;
      transition: transform 0.22s cubic-bezier(0.2, 1, 0.3, 1);
    }
    .btn-cta:hover .btn-arrow {
      transform: translateY(4px);
    }

    .stat-cell {
      cursor: default;
      transition: transform 0.3s cubic-bezier(0.2, 1, 0.3, 1), background-color 0.3s ease;
    }
    .stat-cell:hover {
      transform: translateY(-3px);
      background-color: rgba(255, 255, 255, 0.04) !important;
    }
    .stat-cell:hover .stat-num {
      color: #FFFFFF !important;
    }
    .stat-cell:hover .stat-label {
      color: #FFFFFF !important;
    }
    .stat-num, .stat-label {
      transition: color 0.3s ease;
    }

    .founder-card {
      transition: transform 0.3s cubic-bezier(0.2, 1, 0.3, 1), border-color 0.3s ease, box-shadow 0.3s ease;
      cursor: default;
    }
    .founder-card:hover {
      transform: translateY(-2px);
      border-color: rgba(255, 42, 31, 0.55) !important;
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.45) !important;
    }
    .pulse-dot {
      display: inline-block;
      animation: radarPing 2s cubic-bezier(0.4, 0, 0.2, 1) infinite;
    }

    .profile-row {
      transition: transform 0.25s cubic-bezier(0.2, 1, 0.3, 1), background-color 0.25s ease;
      cursor: default;
    }
    .profile-row:hover {
      transform: translateX(5px);
      background-color: rgba(255, 42, 31, 0.04) !important;
    }
    .profile-row:hover .game-title {
      color: #FFFFFF !important;
    }
    .game-title {
      transition: color 0.2s ease;
    }
    .tagpill {
      display: inline-block;
      transition: transform 0.25s cubic-bezier(0.2, 1, 0.3, 1), box-shadow 0.25s ease, background-color 0.25s ease, color 0.25s ease;
    }
    .profile-row:hover .tagpill {
      transform: scale(1.08);
      background-color: rgba(255, 42, 31, 0.16) !important;
      color: #FFFFFF !important;
      border-color: #FF2A1F !important;
      box-shadow: 0 0 8px rgba(255, 42, 31, 0.3) !important;
    }

    .quickstart-box {
      transition: transform 0.3s cubic-bezier(0.2, 1, 0.3, 1), border-color 0.3s ease, box-shadow 0.3s ease;
    }
    .quickstart-box:hover {
      transform: translateY(-2px);
      border-color: rgba(255, 255, 255, 0.14) !important;
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.5) !important;
    }
    .code-chip {
      transition: background-color 0.2s ease, color 0.2s ease;
    }
    .quickstart-box:hover .code-chip {
      background-color: rgba(255, 42, 31, 0.12) !important;
      color: #FF7B72 !important;
    }

    .footer-link {
      transition: color 0.2s ease;
    }
    .footer-link:hover {
      color: #FF2A1F !important;
    }

    /* Prevent Gmail and Apple Mail from force-coloring autodetected text/numbers to blue */
    a[x-apple-data-detectors],
    a[href^="tel"] {
      color: inherit !important;
      text-decoration: none !important;
    }

    /* Main CTA button link - Pure white text on crimson button */
    .btn-link,
    .btn-link *,
    a.btn-link,
    u + #body a.btn-link,
    #MessageViewBody a.btn-link {
      color: #FFFFFF !important;
      text-decoration: none !important;
    }

    /* Recipient email badge - Crimson red across all clients */
    .email-chip,
    .email-chip *,
    a.email-chip,
    a[href^="mailto"].email-chip,
    u + #body a.email-chip,
    #MessageViewBody a.email-chip {
      color: #FF2A1F !important;
      text-decoration: none !important;
      font-weight: 600 !important;
    }

    /* Footer download link - ALWAYS crimson red in all clients including Gmail */
    .footer-download,
    .footer-download *,
    a.footer-download,
    u + #body a.footer-download,
    #MessageViewBody a.footer-download {
      color: #FF2A1F !important;
      text-decoration: none !important;
      font-weight: 700 !important;
    }

    /* Footer navigation links (PORTAL, TROUBLESHOOTING) */
    .footer-nav-link,
    .footer-nav-link *,
    a.footer-nav-link,
    u + #body a.footer-nav-link,
    #MessageViewBody a.footer-nav-link {
      color: #8A8A92 !important;
      text-decoration: none !important;
    }
  </style>
</head>
<body id="body" style="margin: 0; padding: 0; background-color: #050506; font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; -webkit-font-smoothing: antialiased; color: #F2F2F3;">

  <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" style="background-color: #050506; width: 100%; min-height: 100vh;">
    <tr>
      <td align="center" style="padding: 24px 12px 60px;">
        
        <!-- Main Email Container -->
        <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" class="email-container" style="max-width: 600px; background-color: #0C0C0F; border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 2px; overflow: hidden;">
          
          <!-- Top Crimson Accent Hairline with Subtle Shimmer -->
          <tr>
            <td class="laser-accent" style="height: 2px; background-color: #FF2A1F;"></td>
          </tr>

          <!-- Minimal Header: ONLY "STEREO ENGINE" (No logo icon) -->
          <tr>
            <td align="center" style="padding: 26px 24px 22px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); background-color: #08080B;">
              <div class="header-brand" style="font-family: 'Chakra Petch', sans-serif; font-size: 15px; font-weight: 700; letter-spacing: 0.24em; color: #FFFFFF; text-transform: uppercase;">
                <span style="color: #FF2A1F; font-weight: 400; margin-right: 8px;">—</span>
                ${appName.toUpperCase()}
                <span style="color: #FF2A1F; font-weight: 400; margin-left: 8px;">—</span>
              </div>
            </td>
          </tr>

          <!-- Hero Section -->
          <tr>
            <td style="padding: 38px 28px 24px; text-align: center;">
              
              <!-- Eyebrow with Symmetrical Accent Lines -->
              <div style="font-family: 'Chakra Petch', sans-serif; font-size: 11px; font-weight: 600; letter-spacing: 0.22em; color: #7A7A80; text-transform: uppercase; margin-bottom: 20px;">
                <span style="color: #FF2A1F; margin-right: 6px;">—</span>
                OFFICIAL ENGINE LAUNCH · ${version}
                <span style="color: #FF2A1F; margin-left: 6px;">—</span>
              </div>

              <!-- Big Bold Display Title (Razor Sharp, No Blurry Shadows) -->
              <h1 style="font-family: 'Chakra Petch', sans-serif; font-size: 34px; line-height: 1.08; font-weight: 700; color: #FFFFFF; margin: 0 0 16px; letter-spacing: -0.01em; text-transform: uppercase;">
                ONE CLICK.<br>
                <span class="hero-title-red" style="color: #FF2A1F;">ANY GAME IN VR.</span>
              </h1>

              <p style="font-size: 15px; line-height: 1.65; color: #7A7A80; margin: 0 auto 30px; max-width: 470px;">
                Turn your entire flat PC library into immersive 6DOF VR with a single click. <strong style="color: #FFFFFF;">${appName}</strong> hooks the graphics pipeline while your game runs, reconstructs real-time stereo depth, and hands it straight to your OpenXR headset.
              </p>

              <!-- Primary CTA Button (Clean, Refined, High-Tech Animations) -->
              <table role="presentation" border="0" cellspacing="0" cellpadding="0" align="center">
                <tr>
                  <td align="center" class="btn-cta" style="background-color: #FF2A1F; border: 1px solid #FF2A1F; border-radius: 2px; box-shadow: 0 2px 8px rgba(0, 0, 0, 0.4);">
                    <a href="${downloadUrl}" target="_blank" class="btn-link" style="font-family: 'Chakra Petch', sans-serif; font-size: 13px; font-weight: 600; letter-spacing: 0.16em; text-transform: uppercase; color: #FFFFFF !important; text-decoration: none; padding: 15px 36px; display: inline-block;">
                      <font color="#FFFFFF" style="color: #FFFFFF !important;"><span style="color: #FFFFFF !important;">DOWNLOAD ${appName.toUpperCase()} (${version}) <span class="btn-arrow">↓</span></span></font>
                    </a>
                  </td>
                </tr>
              </table>
              
              <div style="font-family: 'Chakra Petch', sans-serif; font-size: 11px; letter-spacing: 0.08em; text-transform: uppercase; color: #5C5C64; margin-top: 12px;">
                Windows 10 / 11 · 64-bit · OpenXR Compatible
              </div>

            </td>
          </tr>

          <!-- Landing Page Hero Stat Rail (3-Column Cockpit) -->
          <tr>
            <td style="padding: 10px 24px 28px;">
              <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" style="border-top: 1px solid rgba(255, 255, 255, 0.08); border-bottom: 1px solid rgba(255, 255, 255, 0.08); background-color: rgba(255, 255, 255, 0.015);">
                <tr>
                  
                  <!-- Stat 1 -->
                  <td align="center" width="33.33%" class="stat-cell" style="padding: 14px 8px; border-right: 1px solid rgba(255, 255, 255, 0.08);">
                    <div class="stat-num" style="font-family: 'Chakra Petch', sans-serif; font-size: 18px; font-weight: 700; color: #FFFFFF; line-height: 1;">3</div>
                    <div class="stat-label" style="font-family: 'Chakra Petch', sans-serif; font-size: 9px; font-weight: 600; letter-spacing: 0.14em; text-transform: uppercase; color: #7A7A80; margin-top: 5px;">
                      GRAPHICS APIS
                    </div>
                  </td>

                  <!-- Stat 2 -->
                  <td align="center" width="33.33%" class="stat-cell" style="padding: 14px 8px; border-right: 1px solid rgba(255, 255, 255, 0.08);">
                    <div class="stat-num" style="font-family: 'Chakra Petch', sans-serif; font-size: 18px; font-weight: 700; color: #FFFFFF; line-height: 1;">6</div>
                    <div class="stat-label" style="font-family: 'Chakra Petch', sans-serif; font-size: 9px; font-weight: 600; letter-spacing: 0.14em; text-transform: uppercase; color: #7A7A80; margin-top: 5px;">
                      SHIPPED PROFILES
                    </div>
                  </td>

                  <!-- Stat 3 -->
                  <td align="center" width="33.33%" class="stat-cell" style="padding: 14px 8px;">
                    <div class="stat-num" style="font-family: 'Chakra Petch', sans-serif; font-size: 18px; font-weight: 700; color: #FFFFFF; line-height: 1;">11.1<span style="color: #FF2A1F;">ms</span></div>
                    <div class="stat-label" style="font-family: 'Chakra Petch', sans-serif; font-size: 9px; font-weight: 600; letter-spacing: 0.14em; text-transform: uppercase; color: #7A7A80; margin-top: 5px;">
                      FRAME BUDGET
                    </div>
                  </td>

                </tr>
              </table>
            </td>
          </tr>

          <!-- Founder Access Pass Box -->
          <tr>
            <td style="padding: 0 24px 28px;">
              <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" class="founder-card" style="background-color: #101014; border: 1px solid rgba(255, 42, 31, 0.35); border-radius: 2px; padding: 18px 22px;">
                <tr>
                  <td>
                    <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0">
                      <tr>
                        <td style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 700; letter-spacing: 0.18em; color: #FF2A1F; text-transform: uppercase;">
                          <span class="pulse-dot">●</span> PRIORITY ALLOCATED · FOUNDER STATUS
                        </td>
                        <td align="right" style="font-family: ui-monospace, monospace; font-size: 11px; color: #7A7A80;">
                          AUTH: VERIFIED
                        </td>
                      </tr>
                      <tr>
                        <td colspan="2" style="padding-top: 10px;">
                          <div style="font-family: ui-monospace, monospace; font-size: 16px; font-weight: 700; color: #FFFFFF; letter-spacing: 0.08em;">
                            STEREO-FOUNDER-#8492
                          </div>
                        </td>
                      </tr>
                      <tr>
                        <td colspan="2" style="font-size: 12px; color: #8A8A92; padding-top: 6px; line-height: 1.5;">
                          Your priority pass unlocks immediate Tier 1 engine binaries, zero telemetry tracking, and direct access to all shipped engine profiles.
                        </td>
                      </tr>
                    </table>
                  </td>
                </tr>
              </table>
            </td>
          </tr>

          <!-- Shipped Profiles Section -->
          <tr>
            <td style="padding: 0 24px 28px;">
              
              <!-- Section Eyebrow -->
              <div style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.2em; color: #7A7A80; text-transform: uppercase; margin-bottom: 16px; text-align: center;">
                <span style="color: #FF2A1F; margin-right: 6px;">—</span>
                TESTED & TUNED PROFILES
                <span style="color: #FF2A1F; margin-left: 6px;">—</span>
              </div>

              <!-- Profiles List -->
              <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" style="border: 1px solid rgba(255, 255, 255, 0.08); background-color: #0E0E12;">
                
                <tr class="profile-row">
                  <td style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Cyberpunk 2077</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">REDengine 4</span>
                  </td>
                  <td align="right" style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06);">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX12</span>
                  </td>
                </tr>

                <tr class="profile-row">
                  <td style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Elden Ring</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">FromSoftware</span>
                  </td>
                  <td align="right" style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06);">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX12</span>
                  </td>
                </tr>

                <tr class="profile-row">
                  <td style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Sekiro: Shadows Die Twice</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">FromSoftware</span>
                  </td>
                  <td align="right" style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06);">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX11</span>
                  </td>
                </tr>

                <tr class="profile-row">
                  <td style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Hogwarts Legacy</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">Unreal Engine 4</span>
                  </td>
                  <td align="right" style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06);">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX12</span>
                  </td>
                </tr>

                <tr class="profile-row">
                  <td style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06); font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Atomic Heart</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">Unreal Engine 4</span>
                  </td>
                  <td align="right" style="padding: 12px 18px; border-bottom: 1px solid rgba(255, 255, 255, 0.06);">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX12</span>
                  </td>
                </tr>

                <tr class="profile-row">
                  <td style="padding: 12px 18px; font-size: 13px; font-weight: 600; color: #FFFFFF;">
                    <span class="game-title">Palworld</span> <span style="font-size: 11px; color: #7A7A80; font-weight: 400; margin-left: 6px;">Unreal Engine 5</span>
                  </td>
                  <td align="right" style="padding: 12px 18px;">
                    <span class="tagpill" style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.14em; border: 1px solid #FF2A1F; color: #FF2A1F; background-color: rgba(255, 42, 31, 0.08); padding: 3px 7px; border-radius: 2px;">DX12</span>
                  </td>
                </tr>

              </table>
            </td>
          </tr>

          <!-- Quick 3-Step Setup -->
          <tr>
            <td style="padding: 0 24px 32px;">
              <table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="0" class="quickstart-box" style="background-color: #0E0E12; border: 1px solid rgba(255, 255, 255, 0.06); padding: 18px 20px;">
                <tr>
                  <td>
                    <div style="font-family: 'Chakra Petch', sans-serif; font-size: 11px; font-weight: 700; letter-spacing: 0.16em; text-transform: uppercase; color: #FFFFFF; margin-bottom: 10px;">
                      QUICK START IN 3 STEPS:
                    </div>
                    <ol style="margin: 0; padding-left: 18px; font-size: 12px; color: #8A8A92; line-height: 1.75;">
                      <li>Extract the zip to any folder (nothing is written to your game directories).</li>
                      <li>Launch your OpenXR runtime (<strong style="color: #FFFFFF;">Meta Quest Link</strong>, <strong style="color: #FFFFFF;">SteamVR</strong>, or <strong style="color: #FFFFFF;">Virtual Desktop</strong>).</li>
                      <li>Start your game normally, then attach via CLI: <code class="code-chip" style="font-family: ui-monospace, monospace; color: #FFFFFF; background-color: rgba(255,255,255,0.06); padding: 2px 6px; border-radius: 2px;">vr-inject-cli.exe --attach</code></li>
                    </ol>
                  </td>
                </tr>
              </table>
            </td>
          </tr>

          <!-- Footer -->
          <tr>
            <td style="background-color: #08080B; border-top: 1px solid rgba(255, 255, 255, 0.06); padding: 34px 24px 30px; text-align: center;">
              
              <!-- Symmetrical Brand Header -->
              <div style="font-family: 'Chakra Petch', sans-serif; font-size: 13px; font-weight: 700; letter-spacing: 0.22em; text-transform: uppercase; color: #FFFFFF; margin-bottom: 16px;">
                <span style="color: #FF2A1F; font-weight: 400; margin-right: 8px;">—</span>
                ${appName.toUpperCase()}
                <span style="color: #FF2A1F; font-weight: 400; margin-left: 8px;">—</span>
              </div>

              <!-- Recipient Notice with Styled Monospace Chip -->
              <div style="font-size: 11px; line-height: 1.8; color: #7A7A80; margin-bottom: 18px;">
                Sent to <a href="mailto:${recipientEmail}" class="email-chip" style="font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 11px; font-weight: 600; color: #FF2A1F !important; background-color: rgba(255, 42, 31, 0.1); border: 1px solid rgba(255, 42, 31, 0.4); padding: 3px 9px; border-radius: 3px; text-decoration: none !important; display: inline-block; letter-spacing: 0.02em;"><font color="#FF2A1F" style="color: #FF2A1F !important;">${recipientEmail}</font></a> because you joined early access.
              </div>

              <!-- Nav Links in Clean Pill Style -->
              <div style="font-family: 'Chakra Petch', sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.16em; text-transform: uppercase; margin-bottom: 18px;">
                <a href="${websiteUrl}" class="footer-link footer-nav-link" style="color: #8A8A92 !important; text-decoration: none; padding: 4px 10px;"><font color="#8A8A92" style="color: #8A8A92 !important;">PORTAL</font></a>
                <span style="color: rgba(255, 255, 255, 0.15);">·</span>
                <a href="${websiteUrl}#faq" class="footer-link footer-nav-link" style="color: #8A8A92 !important; text-decoration: none; padding: 4px 10px;"><font color="#8A8A92" style="color: #8A8A92 !important;">TROUBLESHOOTING</font></a>
                <span style="color: rgba(255, 255, 255, 0.15);">·</span>
                <a href="${downloadUrl}" class="footer-link footer-download" style="color: #FF2A1F !important; text-decoration: none; padding: 4px 10px; font-weight: 700;"><font color="#FF2A1F" style="color: #FF2A1F !important;"><span style="color: #FF2A1F !important;">DOWNLOAD (${version.toUpperCase()})</span></font></a>
              </div>

              <!-- Center Divider -->
              <div style="height: 1px; width: 44px; background-color: rgba(255, 255, 255, 0.08); margin: 0 auto 16px;"></div>

              <!-- Copyright & Anti-Spam Compliance -->
              <div style="font-size: 10px; line-height: 1.6; color: #5C5C64; letter-spacing: 0.03em;">
                Single-player universal VR injector · © 2026 ${companyName} · Bengaluru, KA / Global<br>
                Early access notification for registered waitlist · <a href="${websiteUrl}#unsubscribe" class="footer-link" style="color: #7A7A80; text-decoration: underline;">Unsubscribe</a>
              </div>

            </td>
          </tr>

        </table>

      </td>
    </tr>
  </table>

</body>
</html>`;
}

export function buildPlainText(recipientEmail, customVersion = null) {
  const version = customVersion || DEFAULT_CONFIG.version;
  const appName = DEFAULT_CONFIG.appName;
  const companyName = DEFAULT_CONFIG.companyName;
  const downloadUrl = DEFAULT_CONFIG.downloadUrl;
  const websiteUrl = DEFAULT_CONFIG.websiteUrl;

  return `${appName.toUpperCase()} — Official Engine Launch ${version}

ONE CLICK. ANY GAME IN VR.
Turn your entire flat PC library into immersive 6DOF VR with a single click. ${appName} hooks the graphics pipeline while your game runs, reconstructs real-time stereo depth, and hands it straight to your OpenXR headset.

Download ${appName} (${version}):
${downloadUrl}

Compatibility: Windows 10 / 11 · 64-bit · OpenXR Compatible
Graphics APIs: 3 (DirectX 11, DirectX 12, Vulkan)
Shipped Profiles: 6
Frame Budget: 11.1ms

Your Founder Access Pass:
Code: STEREO-FOUNDER-#8492
Status: Priority Allocated (Tier 1 engine binaries, zero telemetry tracking)

Tested & Tuned Profiles:
- Cyberpunk 2077 (REDengine 4 · DX12)
- Elden Ring (FromSoftware · DX12)
- Sekiro: Shadows Die Twice (FromSoftware · DX11)
- Hogwarts Legacy (Unreal Engine 4 · DX12)
- Atomic Heart (Unreal Engine 4 · DX12)
- Palworld (Unreal Engine 5 · DX12)

Quick Start in 3 Steps:
1. Extract the zip to any folder.
2. Launch your OpenXR runtime (Meta Quest Link, SteamVR, or Virtual Desktop).
3. Start your game normally, then attach via CLI: vr-inject-cli.exe --attach

Portal: ${websiteUrl}
Troubleshooting: ${websiteUrl}#faq

Sent to ${recipientEmail} because you joined early access.
Single-player universal VR injector. © 2026 ${companyName}.`;
}
