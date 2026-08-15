/* ==========================================================================
   NexVR Engine — console UI behaviour
   --------------------------------------------------------------------------
   Everything degrades to a static, readable page if JS never runs or the
   visitor asks for reduced motion. The markup is complete on its own, except
   the compatibility list, which is data-driven so the skeleton system has
   real work to do.
   ========================================================================== */
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  var prefersStill = function () { return reduceMotion.matches; };

  /* --------------------------------------------------- Scroll reveals */
  var revealObserver = null;
  if ("IntersectionObserver" in window) {
    revealObserver = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-revealed");
        revealObserver.unobserve(entry.target);
      });
    }, { rootMargin: "0px 0px -12% 0px", threshold: 0.12 });
    document.querySelectorAll("[data-reveal]").forEach(function (el) { revealObserver.observe(el); });
  } else {
    document.querySelectorAll("[data-reveal]").forEach(function (el) { el.classList.add("is-revealed"); });
  }

  /* ------------------------------------------------------------- Nav */
  var nav = document.getElementById("nav");
  var navToggle = document.getElementById("nav-toggle");
  var navLinks = document.getElementById("nav-links");

  var onScroll = function () { nav.classList.toggle("is-stuck", window.scrollY > 12); };
  window.addEventListener("scroll", onScroll, { passive: true });
  onScroll();

  navToggle.addEventListener("click", function () {
    var open = navLinks.classList.toggle("is-open");
    navToggle.setAttribute("aria-expanded", String(open));
    navToggle.setAttribute("aria-label", open ? "Close menu" : "Open menu");
  });
  navLinks.addEventListener("click", function (e) {
    if (e.target.closest("a")) {
      navLinks.classList.remove("is-open");
      navToggle.setAttribute("aria-expanded", "false");
    }
  });
  document.addEventListener("keydown", function (e) {
    if (e.key === "Escape" && navLinks.classList.contains("is-open")) {
      navLinks.classList.remove("is-open");
      navToggle.setAttribute("aria-expanded", "false");
      navToggle.focus();
    }
  });

  /* ------------------------------------------------------ Particles */
  function scatterDust(host, x, y, count) {
    if (prefersStill()) return;
    for (var i = 0; i < (count || 12); i++) {
      var bit = document.createElement("span");
      bit.className = "dust" + (i % 4 === 0 ? " dust--pale" : "");
      var a = Math.random() * Math.PI * 2;
      var d = 18 + Math.random() * 55;
      bit.style.left = x + "px";
      bit.style.top = y + "px";
      bit.style.setProperty("--dx", Math.cos(a) * d + "px");
      bit.style.setProperty("--dy", (Math.sin(a) * d - 16) + "px");
      bit.style.animationDelay = (Math.random() * 110) + "ms";
      host.appendChild(bit);
      setTimeout(function (n) { return function () { n.remove(); }; }(bit), 950);
    }
  }

  /* ------------------------------------------- Compatibility index */
  var hall = document.getElementById("hall");

  function pixelIcon(id) {
    return '<svg class="pixel-icon" viewBox="0 0 10 10" aria-hidden="true"><use href="#px-' + id + '"/></svg>';
  }

  var STATUS = {
    tested:   { icon: "check", label: "Tested",  cls: "tag--on" },
    partial:  { icon: "warn",  label: "Partial", cls: "tag--warn" },
    progress: { icon: "gear",  label: "In Dev",  cls: "tag--off" }
  };

  function skeletonCard() {
    var card = document.createElement("div");
    card.className = "panel game game__skel";
    card.setAttribute("aria-hidden", "true");
    card.innerHTML = '<div class="skeleton"><span class="skeleton__label">Loading</span></div>';
    return card;
  }

  function gameCard(game) {
    var status = STATUS[game.status] || STATUS.progress;
    var card = document.createElement("article");
    card.className = "panel game";
    card.dataset.status = game.status;

    var specs = (game.specs || []).map(function (s) {
      return '<span class="meta">' + s + "</span>";
    }).join("");

    card.innerHTML =
      '<div class="game__head">' +
        '<h3 class="game__title">' + game.title + "</h3>" +
        '<span class="tag ' + status.cls + '">' + pixelIcon(status.icon) + " " + status.label + "</span>" +
      "</div>" +
      '<p class="game__meta meta">' + game.year + " · " + game.api + "</p>" +
      '<p class="game__note">' + game.note + "</p>" +
      '<div class="game__foot">' + specs +
        '<span class="meta game__sample" title="Placeholder entry, not a real test result">Sample</span>' +
      "</div>";

    card.addEventListener("pointerenter", function (e) {
      var r = card.getBoundingClientRect();
      scatterDust(card, e.clientX - r.left, e.clientY - r.top, 10);
    });
    return card;
  }

  if (hall) {
    for (var s = 0; s < 6; s++) hall.appendChild(skeletonCard());

    setTimeout(function () {
      hall.textContent = "";
      (window.NEXVR_COMPATIBILITY || []).forEach(function (game, i) {
        var card = gameCard(game);
        hall.appendChild(card);
        if (prefersStill()) card.classList.add("is-revealed");
        else setTimeout(function () { card.classList.add("is-revealed"); }, i * 45);
      });
      hall.setAttribute("aria-busy", "false");
    }, prefersStill() ? 0 : 850);

    document.querySelectorAll(".hall__filters .chip").forEach(function (chip) {
      chip.addEventListener("click", function () {
        document.querySelectorAll(".hall__filters .chip").forEach(function (c) {
          c.classList.toggle("is-active", c === chip);
        });
        var want = chip.dataset.filter;
        hall.querySelectorAll(".game").forEach(function (card) {
          card.classList.toggle("is-filtered", want !== "all" && card.dataset.status !== want);
        });
      });
    });
  }

  /* ------------------------------------------------------- Figures */
  document.querySelectorAll("[data-count]").forEach(function (el) {
    var target = parseInt(el.dataset.count, 10);
    var suffix = el.dataset.suffix || "";
    var fmt = function (n) { return n.toLocaleString("en-US") + suffix; };
    if (prefersStill() || !("IntersectionObserver" in window)) { el.textContent = fmt(target); return; }

    var counter = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        counter.unobserve(el);
        var t0 = performance.now();
        (function tick(now) {
          var p = Math.min(1, (now - t0) / 1100);
          el.textContent = fmt(Math.round(target * (1 - Math.pow(1 - p, 3))));
          if (p < 1) requestAnimationFrame(tick);
        })(t0);
      });
    }, { threshold: 0.4 });
    counter.observe(el);
  });

  /* --------------------------------------------------- Install tabs */
  var tabs = document.querySelectorAll(".tab");
  function selectTab(index) {
    tabs.forEach(function (tab, i) {
      var panel = document.getElementById(tab.getAttribute("aria-controls"));
      var on = i === index;
      tab.classList.toggle("is-active", on);
      tab.setAttribute("aria-selected", String(on));
      panel.classList.toggle("is-hidden", !on);
      panel.hidden = !on;
    });
  }
  tabs.forEach(function (tab, i) {
    tab.addEventListener("click", function () { selectTab(i); });
    tab.addEventListener("keydown", function (e) {
      var next = e.key === "ArrowRight" ? i + 1 : e.key === "ArrowLeft" ? i - 1 : null;
      if (next === null) return;
      e.preventDefault();
      next = (next + tabs.length) % tabs.length;
      selectTab(next);
      tabs[next].focus();
    });
  });

  /* --------------------------------------------------- Copy buttons */
  document.querySelectorAll(".copy").forEach(function (btn) {
    btn.addEventListener("click", function () {
      var text = btn.dataset.copy || "";
      var done = function () {
        btn.textContent = "Copied";
        btn.classList.add("is-done");
        setTimeout(function () { btn.textContent = "Copy"; btn.classList.remove("is-done"); }, 1800);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done, function () { btn.textContent = "Failed"; });
      } else {
        var ta = document.createElement("textarea");
        ta.value = text; document.body.appendChild(ta); ta.select();
        try { document.execCommand("copy"); done(); } catch (err) { btn.textContent = "Failed"; }
        ta.remove();
      }
    });
  });

  /* ======================================================== VIEWPORT
     A real 3D room, projected twice at true interpupillary distance and
     warped through two lens barrels — what a headset frame actually looks
     like, rather than two flat rectangles side by side.

     Pipeline per frame:
       1. draw the room into one wide source buffer, once per eye
       2. read it back as pixels, once
       3. gather through a precomputed distortion LUT into each lens disc
       4. blit the discs onto the visible canvas

     The distortion is a standard radial polynomial (r' = r(1 + k1r² + k2r⁴)).
     "Chromatic aberration" here is a radial two-tap smear that grows toward
     the rim, NOT a per-channel RGB split: a real channel split would invent
     magenta and cyan fringes, and this page has a hard three-colour budget.
     The smear reads as lens softness and stays inside the palette.
     ================================================================== */
  var viewport = document.getElementById("viewport");
  var canvas = document.getElementById("viewport-canvas");

  if (viewport && canvas && canvas.getContext) {
    var ctx = canvas.getContext("2d", { alpha: false });
    var W = canvas.width, H = canvas.height;      // 576 x 288

    /* K2 is capped so the outermost sample, at r * (1 + K1 + K2), still
       lands inside the source buffer — past that the disc rim goes black. */
    var LENS = 224;                                // lens disc, px
    var LR = LENS / 2;                             // lens radius
    var SRC = 320;                                 // per-eye source buffer
    var SR = SRC / 2;
    var K1 = 0.22, K2 = 0.14;                      // barrel coefficients
    var CA = 0.05;                                 // rim smear strength
    var FOCAL = 148;                               // source-space focal length
    var IPD = 0.064;                               // metres, real average

    /* Source: both eyes side by side in one buffer, so the per-frame pixel
       read is a single call instead of two. */
    var src = document.createElement("canvas");
    src.width = SRC * 2; src.height = SRC;
    var sctx = src.getContext("2d", { alpha: false, willReadFrequently: true });

    var lensOut = ctx.createImageData(LENS, LENS);
    var eyeX = [18, W - 18 - LENS];                // lens positions on canvas
    var eyeY = (H - LENS) / 2;

    /* ---- Distortion LUT: output pixel → two source pixels ---- */
    /* lutA = true sample, lutB = smeared sample. lutSplit marks the pixels
       where the two differ enough to be worth blending; inside that radius
       the gather is a straight 32-bit word copy, which is several times
       cheaper than a per-channel blend. The rim vignette is baked into the
       source instead of multiplied here, for the same reason. */
    var lutA = new Int32Array(LENS * LENS).fill(-1);
    var lutB = new Int32Array(LENS * LENS).fill(-1);
    var lutSplit = new Uint8Array(LENS * LENS);

    (function buildLut() {
      for (var oy = 0; oy < LENS; oy++) {
        for (var ox = 0; ox < LENS; ox++) {
          var o = oy * LENS + ox;
          var dx = (ox - LR + 0.5) / LR, dy = (oy - LR + 0.5) / LR;
          var r = Math.sqrt(dx * dx + dy * dy);
          if (r > 1) continue;
          var r2 = r * r;
          var w = 1 + K1 * r2 + K2 * r2 * r2;
          var ax = SR + dx * LR * w, ay = SR + dy * LR * w;
          var wb = w * (1 + CA * r2);
          var bx = SR + dx * LR * wb, by = SR + dy * LR * wb;
          if (ax < 0 || ax >= SRC || ay < 0 || ay >= SRC) continue;
          lutA[o] = (ay | 0) * (SRC * 2) + (ax | 0);
          if (bx >= 0 && bx < SRC && by >= 0 && by < SRC) {
            var ib = (by | 0) * (SRC * 2) + (bx | 0);
            lutB[o] = ib;
            if (ib !== lutA[o]) lutSplit[o] = 1;
          } else {
            lutB[o] = lutA[o];
          }
        }
      }
    })();

    /* ---------------------------- The room ---------------------------- */
    /* Metres. Camera stands at z = 4.2 looking down -Z at a gate at z = -9. */
    var cam = { x: 0, y: 1.6, z: 4.2, yaw: 0, pitch: 0 };
    var tgtYaw = 0, tgtPitch = 0;

    function buildRoom() {
      var L = [];                       // [x1,y1,z1, x2,y2,z2, intensity]
      var i, x, z;
      var X = 4.5, Y = 3.2, Z0 = -9.5, Z1 = 5;

      // Floor + ceiling grid
      for (z = Z0; z <= Z1; z += 1) {
        L.push([-X, 0, z, X, 0, z, z < -6 ? .30 : .55]);
        if (z % 2 === 0) L.push([-X, Y, z, X, Y, z, .10]);
      }
      for (x = -X; x <= X; x += 1.5) {
        L.push([x, 0, Z0, x, 0, Z1, .40]);
        L.push([x, Y, Z0, x, Y, Z1, .07]);
      }
      // Wall uprights
      for (z = Z0; z <= Z1; z += 1.5) {
        L.push([-X, 0, z, -X, Y, z, .22]);
        L.push([ X, 0, z,  X, Y, z, .22]);
      }
      // Wall top/bottom rails
      L.push([-X, 0, Z0, -X, 0, Z1, .5]); L.push([X, 0, Z0, X, 0, Z1, .5]);
      L.push([-X, Y, Z0, -X, Y, Z1, .3]); L.push([X, Y, Z0, X, Y, Z1, .3]);

      // The gate: a bright doorway at the far end
      var gx = 1.35, gy = 2.7, gz = Z0 + .2;
      L.push([-gx, 0, gz, -gx, gy, gz, 1]);
      L.push([ gx, 0, gz,  gx, gy, gz, 1]);
      L.push([-gx, gy, gz, gx, gy, gz, 1]);
      L.push([-gx, 0, gz, gx, 0, gz, 1]);
      for (i = 1; i <= 4; i++) {          // inner rings, receding
        var t = i / 5, ix = gx * (1 - t * .55), iy = gy * (1 - t * .55);
        var iz = gz + t * 1.6;
        L.push([-ix, .1, iz, -ix, iy, iz, .9 - t * .5]);
        L.push([ ix, .1, iz,  ix, iy, iz, .9 - t * .5]);
        L.push([-ix, iy, iz, ix, iy, iz, .9 - t * .5]);
      }

      // Floating crates, for parallax you can actually feel
      function crate(cx, cy, cz, s, gl) {
        var p = [[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]]
          .map(function (v) { return [cx + v[0]*s, cy + v[1]*s, cz + v[2]*s]; });
        [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]]
          .forEach(function (e) { L.push([p[e[0]][0],p[e[0]][1],p[e[0]][2], p[e[1]][0],p[e[1]][1],p[e[1]][2], gl]); });
      }
      crate(-2.7, 0.55, -1.5, 0.55, .75);
      crate( 2.9, 0.45, -4.0, 0.45, .6);
      crate(-3.2, 1.70, -6.2, 0.35, .5);
      crate( 2.2, 1.20,  1.2, 0.30, .8);
      return L;
    }
    var ROOM = buildRoom();

    /* Rotate a world point into camera space, then offset by half the IPD. */
    function toCam(px, py, pz, eyeSign, out) {
      var x = px - cam.x, y = py - cam.y, z = pz - cam.z;
      var cy = Math.cos(-cam.yaw), sy = Math.sin(-cam.yaw);
      var x1 = x * cy - z * sy, z1 = x * sy + z * cy;
      var cp = Math.cos(-cam.pitch), sp = Math.sin(-cam.pitch);
      var y1 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
      out[0] = x1 - eyeSign * IPD * 0.5;
      out[1] = y1;
      out[2] = z2;
    }

    var pa = [0, 0, 0], pb = [0, 0, 0];
    var NEAR = -0.12;

    /* Brightness is quantised into a handful of buckets so the whole room
       draws in ~12 stroke calls instead of one per line. Canvas2D spends far
       more time on draw-call overhead than on the lines themselves, and this
       was the difference between 22fps and a steady 30. */
    var LEVELS = 6;
    var BUCKETS = LEVELS * 2;                      // x2: ice room / red gate
    var segBuf = [], segN = new Int32Array(BUCKETS);
    for (var bi = 0; bi < BUCKETS; bi++) segBuf.push(new Float32Array(ROOM.length * 4));

    function drawRoom(g, ox, oy, eyeSign, focal) {
      var i, k;
      for (i = 0; i < BUCKETS; i++) segN[i] = 0;

      for (i = 0; i < ROOM.length; i++) {
        var L = ROOM[i];
        toCam(L[0], L[1], L[2], eyeSign, pa);
        toCam(L[3], L[4], L[5], eyeSign, pb);
        var az = pa[2], bz = pb[2];
        if (az > NEAR && bz > NEAR) continue;            // fully behind
        var ax = pa[0], ay = pa[1], bx = pb[0], by = pb[1];
        if (az > NEAR) {                                  // clip A to near plane
          var t = (NEAR - bz) / (az - bz);
          ax = bx + (ax - bx) * t; ay = by + (ay - by) * t; az = NEAR;
        } else if (bz > NEAR) {
          var t2 = (NEAR - az) / (bz - az);
          bx = ax + (bx - ax) * t2; by = ay + (by - ay) * t2; bz = NEAR;
        }
        var fa = focal / -az, fb = focal / -bz;

        // Nearer lines burn brighter — cheap depth cue, and very CRT
        var depth = Math.min(1, 6 / Math.max(1.2, (-az - bz) / 2));
        var a = Math.min(1, L[6] * (L[6] >= 0.9 ? 0.62 + depth * 0.8 : 0.32 + depth * 0.6));
        var lvl = Math.min(LEVELS - 1, (a * LEVELS) | 0);
        var key = (L[6] >= 0.9 ? LEVELS : 0) + lvl;
        var buf = segBuf[key], n = segN[key];
        buf[n] = ax * fa; buf[n + 1] = -ay * fa;
        buf[n + 2] = bx * fb; buf[n + 3] = -by * fb;
        segN[key] = n + 4;
      }

      g.save();
      g.beginPath(); g.rect(ox, oy, SRC, SRC); g.clip();
      g.translate(ox + SR, oy + SR);
      g.lineWidth = 1.8;   // survives the nearest-neighbour resample
      g.lineCap = "round";

      for (k = 0; k < BUCKETS; k++) {
        var count = segN[k];
        if (!count) continue;
        var b = segBuf[k];
        var alpha = ((k % LEVELS) + 0.7) / LEVELS;
        var isGate = k >= LEVELS;
        g.strokeStyle = (isGate ? "rgba(217,4,41," : "rgba(232,237,242,")
          + Math.min(1, alpha).toFixed(3) + ")";
        /* Only the gate blooms. shadowBlur is expensive, but it is paid on
           ~16 segments out of ~130, and it is what makes the portal read as
           a light source rather than an outline. */
        if (isGate) { g.shadowColor = "rgba(217,4,41,0.95)"; g.shadowBlur = 10; }
        else if (g.shadowBlur) { g.shadowBlur = 0; g.shadowColor = "transparent"; }
        g.beginPath();
        for (i = 0; i < count; i += 4) {
          g.moveTo(b[i], b[i + 1]);
          g.lineTo(b[i + 2], b[i + 3]);
        }
        g.stroke();
      }
      g.shadowBlur = 0;
      g.restore();
    }

    /* ------------------------------ Frame ------------------------------ */
    var stereo = true;
    var userPaused = false;
    var running = false;
    var lastT = 0, fps = 0, fpsAcc = 0, fpsN = 0;

    var elMode = viewport.querySelector("[data-vp-mode]");
    var elYaw  = viewport.querySelector("[data-vp-yaw]");
    var elFps  = viewport.querySelector("[data-vp-fps]");

    var VOID32 = 0;   // packed 0x070907 with full alpha, set on first use

    /* Baked into the source rather than multiplied per output pixel. The
       falloff is deliberately steep near the rim: it is what makes each eye
       read as a round lens instead of one wide letterbox. */
    function bakeVignette(ox) {
      var g = sctx.createRadialGradient(ox + SR, SR, SR * 0.30, ox + SR, SR, SR * 0.98);
      g.addColorStop(0, "rgba(5,6,10,0)");
      g.addColorStop(0.55, "rgba(5,6,10,0.10)");
      g.addColorStop(0.82, "rgba(5,6,10,0.58)");
      g.addColorStop(1, "rgba(5,6,10,1)");
      sctx.fillStyle = g;
      sctx.fillRect(ox, 0, SRC, SRC);
    }

    function renderStereo() {
      sctx.fillStyle = "#05060A";
      sctx.fillRect(0, 0, SRC * 2, SRC);
      drawRoom(sctx, 0, 0, -1, FOCAL);
      drawRoom(sctx, SRC, 0, 1, FOCAL);
      bakeVignette(0);
      bakeVignette(SRC);

      var raw = sctx.getImageData(0, 0, SRC * 2, SRC);
      var img = raw.data;
      var img32 = new Uint32Array(raw.data.buffer);
      var out = lensOut.data;
      var out32 = new Uint32Array(out.buffer);
      if (!VOID32) { out32[0] = 0; out[0] = 5; out[1] = 6; out[2] = 10; out[3] = 255; VOID32 = out32[0]; }

      ctx.fillStyle = "#05060A";
      ctx.fillRect(0, 0, W, H);

      var N = LENS * LENS;
      for (var e = 0; e < 2; e++) {
        var base = e * SRC;
        for (var o = 0; o < N; o++) {
          var ia = lutA[o];
          if (ia < 0) { out32[o] = VOID32; continue; }
          if (!lutSplit[o]) { out32[o] = img32[ia + base]; continue; }
          /* Rim: two-tap radial smear, weighted toward the true sample.
             Same-hue blend, so nothing off-palette can appear. */
          var sa = (ia + base) * 4, sb = (lutB[o] + base) * 4, q = o * 4;
          out[q]     = img[sa]     * 0.72 + img[sb]     * 0.28;
          out[q + 1] = img[sa + 1] * 0.72 + img[sb + 1] * 0.28;
          out[q + 2] = img[sa + 2] * 0.72 + img[sb + 2] * 0.28;
          out[q + 3] = 255;
        }
        ctx.putImageData(lensOut, eyeX[e], eyeY);
      }
    }

    function renderFlat() {
      ctx.fillStyle = "#05060A";
      ctx.fillRect(0, 0, W, H);
      ctx.save();
      ctx.translate(W / 2 - SR, H / 2 - SR);       // centre the source square
      drawRoom(ctx, 0, 0, 0, FOCAL * 1.15);
      ctx.restore();
    }

    function renderOnce() {
      if (stereo) renderStereo(); else renderFlat();
    }

    /* Runs every animation frame. Measured cost of a full stereo frame —
       both eyes drawn, read back, and gathered through the lens LUT — is
       well under a millisecond, so there is no reason to gate to 30: a
       fixed gate just beats against the display's own cadence and makes
       head tracking feel worse than it is. */
    function frame(now) {
      if (!running) return;
      requestAnimationFrame(frame);
      var dt = now - lastT;
      lastT = now;

      if (dt > 0 && dt < 500) {
        fpsAcc += 1000 / dt; fpsN++;
        if (fpsN >= 20) { fps = Math.round(fpsAcc / fpsN); fpsAcc = 0; fpsN = 0; if (elFps) elFps.textContent = fps; }
      }

      /* Ease toward the pointer target; drift gently when idle. */
      if (!pointerActive) {
        tgtYaw = Math.sin(now / 4200) * 0.16;
        tgtPitch = Math.sin(now / 6100) * 0.05;
      }
      cam.yaw += (tgtYaw - cam.yaw) * 0.08;
      cam.pitch += (tgtPitch - cam.pitch) * 0.08;
      if (elYaw) elYaw.textContent = (cam.yaw * 180 / Math.PI).toFixed(0) + "°";

      renderOnce();
    }

    function start() {
      if (running || userPaused || prefersStill()) return;
      running = true; lastT = performance.now();
      requestAnimationFrame(frame);
    }
    function stop() { running = false; }

    /* ------------------------------ Input ------------------------------ */
    var pointerActive = false, pointerTimer = null;

    function aim(clientX, clientY) {
      var r = viewport.getBoundingClientRect();
      var nx = (clientX - r.left) / r.width - 0.5;
      var ny = (clientY - r.top) / r.height - 0.5;
      tgtYaw = -nx * 0.75;
      tgtPitch = -ny * 0.32;
      pointerActive = true;
      clearTimeout(pointerTimer);
      pointerTimer = setTimeout(function () { pointerActive = false; }, 2200);
      if (userPaused || prefersStill()) renderOnce();   // still respond when paused
    }

    viewport.addEventListener("pointermove", function (e) { aim(e.clientX, e.clientY); });
    viewport.addEventListener("pointerleave", function () { pointerActive = false; });

    /* ---------------------------- Controls ---------------------------- */
    var eyesBtn = viewport.querySelector("[data-vp-eyes]");
    var playBtn = viewport.querySelector("[data-vp-play]");

    if (eyesBtn) {
      eyesBtn.addEventListener("click", function () {
        stereo = !stereo;
        eyesBtn.textContent = stereo ? "Stereo" : "Flat";
        eyesBtn.setAttribute("aria-pressed", String(stereo));
        if (elMode) elMode.textContent = stereo ? "Stereo" : "Flat";
        var r = viewport.getBoundingClientRect();
        scatterDust(viewport, r.width / 2, r.height / 2, 16);
        renderOnce();
      });
    }

    if (playBtn) {
      var setPaused = function (paused) {
        userPaused = paused;
        playBtn.setAttribute("aria-pressed", String(paused));
        playBtn.textContent = paused ? "Play" : "Pause";
        if (paused) stop(); else start();
      };
      playBtn.addEventListener("click", function () { setPaused(!userPaused); });
      if (prefersStill()) {
        userPaused = true;
        playBtn.setAttribute("aria-pressed", "true");
        playBtn.textContent = "Play";
      }
    }

    /* ---------------------------- Lifecycle ---------------------------- */
    function ready() {
      viewport.classList.add("is-ready");
      renderOnce();
      if (elFps) elFps.textContent = prefersStill() ? "static" : "—";
      if (!prefersStill()) start();
    }
    setTimeout(ready, 650);

    if ("IntersectionObserver" in window) {
      new IntersectionObserver(function (entries) {
        entries.forEach(function (e) {
          if (prefersStill()) return;
          if (e.isIntersecting) start(); else stop();
        });
      }, { threshold: 0.12 }).observe(viewport);
    }

    document.addEventListener("visibilitychange", function () {
      if (document.hidden) stop(); else if (!prefersStill()) start();
    });

    reduceMotion.addEventListener("change", function () {
      var btn = viewport.querySelector("[data-vp-play]");
      if (prefersStill()) {
        stop(); renderOnce();
        userPaused = true;
        if (btn) { btn.setAttribute("aria-pressed", "true"); btn.textContent = "Play"; }
      } else {
        userPaused = false;
        if (btn) { btn.setAttribute("aria-pressed", "false"); btn.textContent = "Pause"; }
        start();
      }
    });
  }
})();
