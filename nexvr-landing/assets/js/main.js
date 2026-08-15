/* ==========================================================================
   NexVR Engine — "Gilded Arcade" behaviour
   --------------------------------------------------------------------------
   Every effect here degrades to a static, readable page if JS never runs or
   if the visitor asks for reduced motion. Nothing below is load-bearing for
   content — the markup is complete on its own, except the Compatibility Hall,
   which is data-driven on purpose so the skeleton system has real work to do.
   ========================================================================== */
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  var prefersStill = function () { return reduceMotion.matches; };

  /* ---------------------------------------------- Filigree corner marks */
  /* Injected rather than hand-written into every card: the ornament is
     decorative trim, and keeping it out of the markup keeps the document
     readable for both authors and screen readers. */
  var CORNERS = ["tl", "tr", "bl", "br"];

  function dressPlaque(plaque) {
    if (plaque.dataset.dressed) return;
    plaque.dataset.dressed = "1";
    CORNERS.forEach(function (pos) {
      var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
      svg.setAttribute("class", "fil-c fil-c--" + pos);
      svg.setAttribute("viewBox", "0 0 28 28");
      svg.setAttribute("aria-hidden", "true");
      svg.setAttribute("focusable", "false");
      var use = document.createElementNS("http://www.w3.org/2000/svg", "use");
      use.setAttribute("href", "#fil-corner");
      svg.appendChild(use);
      plaque.appendChild(svg);
    });
  }

  function dressAll(root) {
    (root || document).querySelectorAll(".plaque").forEach(dressPlaque);
  }
  dressAll();

  /* --------------------------------------------------- Etch-in reveals */
  /* The filigree border draws itself first (CSS transitions on the frame),
     then the content fades up inside it a beat later. */
  var revealObserver = null;

  if ("IntersectionObserver" in window) {
    revealObserver = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-revealed");
        revealObserver.unobserve(entry.target);
      });
    }, { rootMargin: "0px 0px -12% 0px", threshold: 0.12 });

    document.querySelectorAll("[data-reveal]").forEach(function (el) {
      revealObserver.observe(el);
    });
  } else {
    document.querySelectorAll("[data-reveal]").forEach(function (el) {
      el.classList.add("is-revealed");
    });
  }

  /* ------------------------------------------------------------- Nav */
  var nav = document.getElementById("nav");
  var navToggle = document.getElementById("nav-toggle");
  var navLinks = document.getElementById("nav-links");

  var onScroll = function () {
    nav.classList.toggle("is-stuck", window.scrollY > 12);
  };
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

  /* ------------------------------------------------------ Pixel dust */
  /* A flat surface dissolving into VR through a shower of pixels — the same
     story the portal tells, scaled down to a hover. */
  function scatterDust(host, originX, originY, count) {
    if (prefersStill()) return;
    var n = count || 14;
    for (var i = 0; i < n; i++) {
      var bit = document.createElement("span");
      bit.className = "dust" + (i % 3 === 0 ? " dust--gold" : "");
      var angle = Math.random() * Math.PI * 2;
      var dist = 20 + Math.random() * 60;
      bit.style.left = originX + "px";
      bit.style.top = originY + "px";
      bit.style.setProperty("--dx", Math.cos(angle) * dist + "px");
      bit.style.setProperty("--dy", (Math.sin(angle) * dist - 20) + "px");
      bit.style.animationDelay = (Math.random() * 120) + "ms";
      host.appendChild(bit);
      /* eslint-disable-next-line no-loop-func */
      setTimeout(function (node) { return function () { node.remove(); }; }(bit), 950);
    }
  }

  /* ----------------------------------------------- Compatibility Hall */
  var hall = document.getElementById("hall");

  function pixelIcon(id) {
    return '<svg class="pixel-icon" viewBox="0 0 10 10" aria-hidden="true"><use href="#px-' + id + '"/></svg>';
  }

  var STATUS = {
    tested:   { icon: "check", label: "TESTED",  cls: "pixel-tag--ok" },
    partial:  { icon: "warn",  label: "PARTIAL", cls: "pixel-tag--warn" },
    progress: { icon: "gear",  label: "IN DEV",  cls: "pixel-tag--wip" }
  };

  function skeletonCard() {
    var card = document.createElement("div");
    card.className = "plaque game game__skel";
    card.setAttribute("aria-hidden", "true");
    card.innerHTML =
      '<div class="skeleton" style="height:170px">' +
      '<span class="skeleton__label">LOADING…</span>' +
      "</div>";
    return card;
  }

  function gameCard(game) {
    var status = STATUS[game.status] || STATUS.progress;
    var card = document.createElement("article");
    card.className = "plaque game";
    card.dataset.status = game.status;

    var specs = (game.specs || []).map(function (s) {
      return '<span class="game__spec">' + s + "</span>";
    }).join("");

    card.innerHTML =
      '<div class="game__head">' +
        '<h3 class="game__title">' + game.title + "</h3>" +
        '<span class="pixel-tag ' + status.cls + '">' + pixelIcon(status.icon) + " " + status.label + "</span>" +
      "</div>" +
      '<p class="game__meta">' + game.year + " · " + game.api + "</p>" +
      '<p class="game__note">' + game.note + "</p>" +
      '<div class="game__foot">' + specs +
        '<span class="game__sample" title="Placeholder entry, not a real test result">Sample</span>' +
      "</div>";

    dressPlaque(card);

    card.addEventListener("pointerenter", function (e) {
      var rect = card.getBoundingClientRect();
      scatterDust(card, e.clientX - rect.left, e.clientY - rect.top, 12);
    });

    return card;
  }

  function renderHall(games) {
    hall.textContent = "";
    games.forEach(function (game, i) {
      var card = gameCard(game);
      hall.appendChild(card);
      if (prefersStill()) {
        card.classList.add("is-revealed");
      } else {
        setTimeout(function () { card.classList.add("is-revealed"); }, i * 45);
      }
    });
    hall.setAttribute("aria-busy", "false");
  }

  if (hall) {
    /* Skeletons paint immediately so the hall has structure before data. */
    for (var s = 0; s < 6; s++) hall.appendChild(skeletonCard());

    var games = window.NEXVR_COMPATIBILITY || [];
    /* Stands in for the real index fetch; swap for fetch() at launch. */
    setTimeout(function () { renderHall(games); }, prefersStill() ? 0 : 850);

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

    if (prefersStill() || !("IntersectionObserver" in window)) {
      el.textContent = fmt(target);
      return;
    }

    var counter = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        counter.unobserve(el);
        var start = performance.now();
        var dur = 1100;
        (function tick(now) {
          var p = Math.min(1, (now - start) / dur);
          var eased = 1 - Math.pow(1 - p, 3);
          el.textContent = fmt(Math.round(target * eased));
          if (p < 1) requestAnimationFrame(tick);
        })(start);
      });
    }, { threshold: 0.4 });
    counter.observe(el);
  });

  /* ---------------------------------------------------- Install tabs */
  var tabs = document.querySelectorAll(".tab");
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
        navigator.clipboard.writeText(text).then(done, function () { btn.textContent = "Copy failed"; });
      } else {
        var ta = document.createElement("textarea");
        ta.value = text;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand("copy"); done(); } catch (err) { btn.textContent = "Copy failed"; }
        ta.remove();
      }
    });
  });

  /* --------------------------------------------------- Pixel cursor */
  var cursorToggle = document.getElementById("cursor-toggle");
  if (cursorToggle) {
    var fine = window.matchMedia("(pointer: fine)").matches;
    if (!fine) {
      cursorToggle.hidden = true;
    } else {
      var stored = null;
      try { stored = localStorage.getItem("nexvr-pixel-cursor"); } catch (e) { /* private mode */ }
      var setCursor = function (on) {
        document.body.classList.toggle("pixel-cursor", on);
        cursorToggle.setAttribute("aria-pressed", String(on));
        try { localStorage.setItem("nexvr-pixel-cursor", on ? "1" : "0"); } catch (e) { /* ignore */ }
      };
      setCursor(stored === "1");
      cursorToggle.addEventListener("click", function () {
        setCursor(cursorToggle.getAttribute("aria-pressed") !== "true");
      });
    }
  }

  /* ============================================================ PORTAL
     A flat frame dissolving into a stereo pair through chunky pixels.
     The scene is drawn procedurally at low resolution and upscaled with
     nearest-neighbour, so every particle is a real pixel block, not a blur.
     ================================================================== */
  var portal = document.getElementById("portal");
  var canvas = document.getElementById("portal-canvas");

  if (portal && canvas && canvas.getContext) {
    var ctx = canvas.getContext("2d", { alpha: false });
    var W = canvas.width, H = canvas.height;
    var BLOCK = 10;                       // particle size in canvas pixels
    var COLS = Math.ceil(W / BLOCK);
    var ROWS = Math.ceil(H / BLOCK);

    var flat = makeBuffer(W, H);
    var stereo = makeBuffer(W, H);

    /* Per-block dissolve thresholds — a stable random field so the wipe
       looks scattered rather than swept. */
    var thresholds = new Float32Array(COLS * ROWS);
    for (var t = 0; t < thresholds.length; t++) thresholds[t] = Math.random();

    function makeBuffer(w, h) {
      var c = document.createElement("canvas");
      c.width = w; c.height = h;
      return { canvas: c, ctx: c.getContext("2d", { alpha: false }) };
    }

    /* ---- Scene: a stylised vista in brand colours, drawn per eye ---- */
    function drawScene(g, x, y, w, h, time, eye) {
      var parallax = eye * (w * 0.012);   // eye = -1 | 0 | +1
      var bob = Math.sin(time / 1400) * (h * 0.012);

      g.save();
      g.beginPath();
      g.rect(x, y, w, h);
      g.clip();
      g.translate(x, y + bob);

      // Sky
      var sky = g.createLinearGradient(0, 0, 0, h);
      sky.addColorStop(0, "#0B0A0C");
      sky.addColorStop(0.55, "#1C1620");
      sky.addColorStop(1, "#2a1d24");
      g.fillStyle = sky;
      g.fillRect(-w, -h, w * 3, h * 3);

      // Gate disc
      var cx = w / 2 - parallax * 0.3, cy = h * 0.46;
      var r = h * 0.2;
      var halo = g.createRadialGradient(cx, cy, r * 0.2, cx, cy, r * 2.6);
      halo.addColorStop(0, "rgba(198,161,91,.55)");
      halo.addColorStop(0.4, "rgba(198,161,91,.14)");
      halo.addColorStop(1, "rgba(198,161,91,0)");
      g.fillStyle = halo;
      g.fillRect(-w, -h, w * 3, h * 3);

      g.fillStyle = "#C6A15B";
      g.beginPath(); g.arc(cx, cy, r, 0, Math.PI * 2); g.fill();
      g.fillStyle = "rgba(79,243,232,.85)";
      g.beginPath(); g.arc(cx, cy, r * (0.34 + Math.sin(time / 700) * 0.03), 0, Math.PI * 2); g.fill();

      // Columns flanking the gate
      g.fillStyle = "#0E0B11";
      var colW = w * 0.055;
      [-1, 1].forEach(function (side) {
        var colX = cx + side * (w * 0.19) - colW / 2 - parallax * 0.9;
        g.fillRect(colX, h * 0.24, colW, h * 0.44);
        g.fillStyle = "rgba(198,161,91,.35)";
        g.fillRect(colX, h * 0.24, colW, 3);   // gilded capital
        g.fillStyle = "#0E0B11";
      });

      // Ridge line
      g.fillStyle = "#120E16";
      g.beginPath();
      g.moveTo(-w, h * 0.68);
      for (var i = -4; i <= 24; i++) {
        var px = (i / 20) * w - parallax * 1.6;
        var py = h * 0.68 - Math.abs(Math.sin(i * 1.7)) * h * 0.13;
        g.lineTo(px, py);
      }
      g.lineTo(w * 2, h);
      g.lineTo(-w, h);
      g.closePath();
      g.fill();

      // Perspective floor grid — the cue that sells depth per eye
      g.strokeStyle = "rgba(79,243,232,.32)";
      g.lineWidth = 1;
      var horizon = h * 0.68;
      for (var k = 1; k <= 9; k++) {
        var gy = horizon + Math.pow(k / 9, 2.2) * (h - horizon);
        g.beginPath(); g.moveTo(0, gy); g.lineTo(w, gy); g.stroke();
      }
      g.strokeStyle = "rgba(198,161,91,.22)";
      for (var m = -7; m <= 7; m++) {
        var vx = w / 2 + m * (w * 0.06) - parallax * 2.4;
        g.beginPath();
        g.moveTo(w / 2 - parallax * 0.4, horizon);
        g.lineTo(vx + m * (w * 0.14), h);
        g.stroke();
      }
      g.restore();
    }

    function renderFlat(time) {
      drawScene(flat.ctx, 0, 0, W, H, time, 0);
    }

    function renderStereo(time) {
      var g = stereo.ctx;
      g.fillStyle = "#07060a";
      g.fillRect(0, 0, W, H);
      var gap = 8;
      var vw = (W - gap * 3) / 2;
      var vh = H - gap * 2;
      drawScene(g, gap, gap, vw, vh, time, -1);
      drawScene(g, gap * 2 + vw, gap, vw, vh, time, 1);

      // Lens vignette per eye
      [gap, gap * 2 + vw].forEach(function (vx) {
        var vg = g.createRadialGradient(vx + vw / 2, gap + vh / 2, vh * 0.25, vx + vw / 2, gap + vh / 2, vh * 0.72);
        vg.addColorStop(0, "rgba(0,0,0,0)");
        vg.addColorStop(1, "rgba(0,0,0,.72)");
        g.fillStyle = vg;
        g.fillRect(vx, gap, vw, vh);
      });
    }

    /* ---- State machine: hold flat → dissolve → hold stereo → back ---- */
    var PHASE = { FLAT: 0, TO_STEREO: 1, STEREO: 2, TO_FLAT: 3 };
    var HOLD = 3200, WIPE = 1400;
    var phase = PHASE.FLAT;
    var phaseStart = 0;
    var hudLabel = document.querySelector("[data-portal-state]");

    function setHud(text) {
      if (hudLabel && hudLabel.textContent !== text) hudLabel.textContent = text;
    }

    function drawDissolve(from, to, p) {
      ctx.drawImage(from.canvas, 0, 0);
      var wave = 0.14;
      for (var row = 0; row < ROWS; row++) {
        for (var col = 0; col < COLS; col++) {
          var th = thresholds[row * COLS + col];
          var x = col * BLOCK, y = row * BLOCK;
          if (th <= p - wave) {
            ctx.drawImage(to.canvas, x, y, BLOCK, BLOCK, x, y, BLOCK, BLOCK);
          } else if (th <= p) {
            // Wavefront: the pixel is in flight — offset and tinted cyan
            var f = (p - th) / wave;
            var jx = Math.round((1 - f) * (th - 0.5) * 44);
            var jy = Math.round((1 - f) * -18);
            ctx.drawImage(to.canvas, x, y, BLOCK, BLOCK, x + jx, y + jy, BLOCK, BLOCK);
            ctx.fillStyle = "rgba(79,243,232," + (0.55 * (1 - f)).toFixed(3) + ")";
            ctx.fillRect(x + jx, y + jy, BLOCK, BLOCK);
          }
        }
      }
    }

    var running = false;
    var lastFrame = 0;
    var userPaused = false;   // set by the Pause control; outranks visibility

    function frame(now) {
      if (!running) return;
      requestAnimationFrame(frame);
      if (now - lastFrame < 1000 / 30) return;   // 30fps is plenty, and cheap
      lastFrame = now;

      if (!phaseStart) phaseStart = now;
      var elapsed = now - phaseStart;

      renderFlat(now);
      renderStereo(now);

      if (phase === PHASE.FLAT) {
        ctx.drawImage(flat.canvas, 0, 0);
        setHud("FLAT · 1 VIEW");
        if (elapsed > HOLD) { phase = PHASE.TO_STEREO; phaseStart = now; }
      } else if (phase === PHASE.TO_STEREO) {
        var p1 = Math.min(1, elapsed / WIPE);
        drawDissolve(flat, stereo, p1 * 1.14);
        setHud("INJECTING…");
        if (p1 >= 1) { phase = PHASE.STEREO; phaseStart = now; }
      } else if (phase === PHASE.STEREO) {
        ctx.drawImage(stereo.canvas, 0, 0);
        setHud("STEREO · 2 VIEWS");
        if (elapsed > HOLD) { phase = PHASE.TO_FLAT; phaseStart = now; }
      } else {
        var p2 = Math.min(1, elapsed / WIPE);
        drawDissolve(stereo, flat, p2 * 1.14);
        setHud("RELEASING…");
        if (p2 >= 1) { phase = PHASE.FLAT; phaseStart = now; }
      }
    }

    function start() {
      if (running || userPaused || prefersStill()) return;
      running = true;
      lastFrame = 0;
      requestAnimationFrame(frame);
    }
    function stop() { running = false; }

    /* Advance one phase without waiting for the hold to elapse. */
    function stepPhase() {
      if (phase === PHASE.FLAT || phase === PHASE.TO_FLAT) phase = PHASE.TO_STEREO;
      else phase = PHASE.TO_FLAT;
      phaseStart = 0;
      if (userPaused || prefersStill()) {
        /* Paused: jump straight to the destination rather than animating. */
        phase = phase === PHASE.TO_STEREO ? PHASE.STEREO : PHASE.FLAT;
        var buf = phase === PHASE.STEREO ? stereo : flat;
        (phase === PHASE.STEREO ? renderStereo : renderFlat)(performance.now());
        ctx.drawImage(buf.canvas, 0, 0);
        setHud(phase === PHASE.STEREO ? "STEREO · 2 VIEWS" : "FLAT · 1 VIEW");
      }
    }

    function paintStill() {
      renderStereo(0);
      ctx.drawImage(stereo.canvas, 0, 0);
      setHud("STEREO · 2 VIEWS");
    }

    /* Reveal the canvas once the first frame exists — the skeleton holds the
       space until then, which is exactly what it is for. */
    function ready() {
      portal.classList.add("is-ready");
      if (prefersStill()) paintStill(); else start();
    }
    setTimeout(ready, 700);

    if ("IntersectionObserver" in window) {
      new IntersectionObserver(function (entries) {
        entries.forEach(function (e) {
          if (prefersStill()) return;
          if (e.isIntersecting) start(); else stop();
        });
      }, { threshold: 0.15 }).observe(portal);
    }

    document.addEventListener("visibilitychange", function () {
      if (document.hidden) stop();
      else if (!prefersStill()) start();
    });

    reduceMotion.addEventListener("change", function () {
      var btn = portal.querySelector("[data-portal-play]");
      if (prefersStill()) {
        stop();
        paintStill();
        userPaused = true;
        if (btn) { btn.setAttribute("aria-pressed", "true"); btn.textContent = "Play"; }
      } else {
        /* Clear the pause this listener set, so the control isn't stuck. */
        userPaused = false;
        if (btn) { btn.setAttribute("aria-pressed", "false"); btn.textContent = "Pause"; }
        start();
      }
    });

    /* Controls. The canvas animates on its own, so it needs a real stop —
       and every action has to work without a pointer, which a click handler
       on the canvas alone does not. Both are buttons in the HUD. */
    var stepBtn = portal.querySelector("[data-portal-step]");
    var playBtn = portal.querySelector("[data-portal-play]");

    if (stepBtn) {
      stepBtn.addEventListener("click", function () {
        stepPhase();
        var r = portal.getBoundingClientRect();
        scatterDust(portal, r.width / 2, r.height / 2, 18);
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

      /* Under reduced motion the loop never runs, so the control would be a
         lie — present it as already stopped and let it drive stepPhase only. */
      if (prefersStill()) {
        userPaused = true;
        playBtn.setAttribute("aria-pressed", "true");
        playBtn.textContent = "Play";
      }
    }

    /* Clicking the canvas stays as a pointer shortcut; the buttons above are
       the accessible path, so this is an extra rather than the only way in. */
    portal.addEventListener("click", function (e) {
      if (e.target.closest(".portal__btn")) return;
      var rect = portal.getBoundingClientRect();
      scatterDust(portal, e.clientX - rect.left, e.clientY - rect.top, 18);
      stepPhase();
    });
  }
})();
