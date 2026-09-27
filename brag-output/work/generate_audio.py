import wave
import struct
import math
import random

SAMPLE_RATE = 44100
DURATION = 20.0
NUM_SAMPLES = int(SAMPLE_RATE * DURATION)
BPM = 130.0
BEAT_SEC = 60.0 / BPM

left_channel = [0.0] * NUM_SAMPLES
right_channel = [0.0] * NUM_SAMPLES

def clamp(v, low=-1.0, high=1.0):
    return max(low, min(high, v))

def add_sample(t_idx, l, r):
    if 0 <= t_idx < NUM_SAMPLES:
        left_channel[t_idx] += l
        right_channel[t_idx] += r

# 1. 808 Sub-Kick
def kick(start_sec, gain=0.85):
    start_idx = int(start_sec * SAMPLE_RATE)
    kick_len = int(0.35 * SAMPLE_RATE)
    for i in range(kick_len):
        t = i / SAMPLE_RATE
        freq = 45.0 + 115.0 * math.exp(-t * 24.0)
        phase = 2.0 * math.pi * (45.0 * t - (115.0 / 24.0) * (math.exp(-t * 24.0) - 1.0))
        env = math.exp(-t * 9.0)
        click = (random.random() * 2.0 - 1.0) * math.exp(-t * 150.0) * 0.25
        val = (math.sin(phase) + click) * env * gain
        add_sample(start_idx + i, val, val)

# 2. Snare / Cyberpunk Clap
def snare(start_sec, gain=0.6):
    start_idx = int(start_sec * SAMPLE_RATE)
    snare_len = int(0.22 * SAMPLE_RATE)
    for i in range(snare_len):
        t = i / SAMPLE_RATE
        noise = (random.random() * 2.0 - 1.0) * math.exp(-t * 20.0)
        tone = math.sin(2.0 * math.pi * 185.0 * t) * math.exp(-t * 25.0) * 0.4
        tone2 = math.sin(2.0 * math.pi * 330.0 * t) * math.exp(-t * 35.0) * 0.2
        val = (noise + tone + tone2) * gain
        add_sample(start_idx + i, val * 0.95, val * 1.05)

# 3. Closed Hi-Hat
def hihat(start_sec, gain=0.22, pan=0.0):
    start_idx = int(start_sec * SAMPLE_RATE)
    hh_len = int(0.05 * SAMPLE_RATE)
    for i in range(hh_len):
        t = i / SAMPLE_RATE
        noise = (random.random() * 2.0 - 1.0) * math.exp(-t * 80.0) * gain
        add_sample(start_idx + i, noise * (1.0 - pan * 0.5), noise * (1.0 + pan * 0.5))

# 4. Cyberpunk Bass (Warm distorted saw + sub)
def bass_note(start_sec, dur_sec, freq, gain=0.45, pan=0.0):
    start_idx = int(start_sec * SAMPLE_RATE)
    note_len = int(dur_sec * SAMPLE_RATE)
    for i in range(note_len):
        t = i / SAMPLE_RATE
        phase = (t * freq) % 1.0
        saw = 2.0 * phase - 1.0
        sub = math.sin(2.0 * math.pi * (freq * 0.5) * t) * 0.6
        raw = saw * 0.5 + sub
        dist = math.tanh(raw * 1.6)
        env = math.exp(-t * 5.0) if dur_sec < 0.3 else min(1.0, t * 100.0) * max(0.0, 1.0 - t / dur_sec)
        val = dist * env * gain
        add_sample(start_idx + i, val * (1.0 - pan), val * (1.0 + pan))

# 5. Neon Pluck Synth Arp
def pluck(start_sec, dur_sec, freq, gain=0.28, pan=0.0):
    start_idx = int(start_sec * SAMPLE_RATE)
    note_len = int(dur_sec * SAMPLE_RATE)
    for i in range(note_len):
        t = i / SAMPLE_RATE
        val = (math.sin(2.0 * math.pi * freq * t) + 
               0.45 * math.sin(2.0 * math.pi * freq * 2.0 * t) + 
               0.2 * math.sin(2.0 * math.pi * freq * 3.0 * t))
        env = math.exp(-t * 14.0)
        sample = val * env * gain
        add_sample(start_idx + i, sample * (1.0 - pan * 0.5), sample * (1.0 + pan * 0.5))
        # Ping pong stereo delay at 180ms
        delay_idx = start_idx + i + int(0.18 * SAMPLE_RATE)
        add_sample(delay_idx, sample * 0.35 * (1.0 + pan * 0.5), sample * 0.35 * (1.0 - pan * 0.5))

# 6. Cinematic Sub Impact
def cinematic_impact(start_sec, gain=0.9):
    start_idx = int(start_sec * SAMPLE_RATE)
    boom_len = int(2.8 * SAMPLE_RATE)
    for i in range(boom_len):
        t = i / SAMPLE_RATE
        phase = 2.0 * math.pi * (32.0 * t - (70.0 / 3.0) * (math.exp(-t * 3.0) - 1.0))
        sub = math.sin(phase) * math.exp(-t * 1.1)
        noise = (random.random() * 2.0 - 1.0) * math.exp(-t * 10.0) * 0.3
        shimmer = math.sin(2.0 * math.pi * 2200.0 * t) * math.exp(-t * 3.5) * 0.1
        val = (sub + noise + shimmer) * gain
        add_sample(start_idx + i, val * 0.98, val * 1.02)

# 7. Whoosh / Riser
def riser(start_sec, dur_sec, start_freq, end_freq, gain=0.35):
    start_idx = int(start_sec * SAMPLE_RATE)
    riser_len = int(dur_sec * SAMPLE_RATE)
    for i in range(riser_len):
        progress = i / riser_len
        cur_t = i / SAMPLE_RATE
        cur_freq = start_freq + (end_freq - start_freq) * (progress ** 2.0)
        phase = 2.0 * math.pi * cur_freq * cur_t
        osc = math.sin(phase) * 0.4
        noise = (random.random() * 2.0 - 1.0) * 0.6
        env = (progress ** 1.8) * gain
        pan = math.sin(progress * math.pi * 3.0) * 0.4
        val = (osc + noise) * env
        add_sample(start_idx + i, val * (1.0 - pan), val * (1.0 + pan))

# ========================================================
# COMPOSITION TIMELINE (0.0s to 20.0s)
# ========================================================

# Scene 1: The Hook (0.0s - 3.2s)
cinematic_impact(0.05, gain=0.95)
intro_notes = [73.4, 73.4, 65.4, 55.0]
for idx, n in enumerate(intro_notes):
    bass_note(idx * 0.75, 0.7, n, gain=0.42, pan=-0.2 if idx%2==0 else 0.2)
riser(1.2, 2.0, 150.0, 1600.0, gain=0.35)

# Scene 2, 3, 4: Driving Cyberpunk Groove (3.2s to 16.5s)
start_groove = 3.23
end_groove = 16.5

d2, f2, g2, c2, a1 = 73.4, 87.3, 98.0, 65.4, 55.0
bass_pattern = [
    d2, d2, d2, f2,  d2, d2, g2, f2,
    d2, d2, d2, c2,  d2, d2, a1, c2
]

arp_freqs = [
    293.66, 349.23, 440.00, 523.25, 587.33, 698.46, 523.25, 440.00,
    349.23, 293.66, 440.00, 587.33, 698.46, 880.00, 698.46, 587.33
]

beat_count = int((end_groove - start_groove) / BEAT_SEC)

for b in range(beat_count):
    beat_t = start_groove + b * BEAT_SEC
    if beat_t >= end_groove:
        break
    
    # 4-on-the-floor Kick
    kick(beat_t, gain=0.85)
    
    # Snare on 2 and 4
    if b % 2 == 1:
        snare(beat_t, gain=0.62)
        
    # 16th note hi-hats
    for sub in range(4):
        hh_t = beat_t + sub * (BEAT_SEC / 4.0)
        pan = -0.3 if sub % 2 == 0 else 0.3
        accent = 0.32 if sub == 2 else 0.18
        hihat(hh_t, gain=accent, pan=pan)
        
    # 16th note rolling bass
    for sub in range(4):
        b_idx = (b * 4 + sub) % len(bass_pattern)
        b_freq = bass_pattern[b_idx]
        b_t = beat_t + sub * (BEAT_SEC / 4.0)
        bass_note(b_t, BEAT_SEC / 4.0 * 0.9, b_freq, gain=0.38, pan=(sub - 1.5) * 0.12)
        
    # Arpeggios (starting scene 3: t >= 7.5s)
    if beat_t >= 7.5:
        for sub in range(4):
            a_idx = (b * 4 + sub) % len(arp_freqs)
            a_freq = arp_freqs[a_idx]
            a_t = beat_t + sub * (BEAT_SEC / 4.0)
            pan = 0.4 if sub % 2 == 0 else -0.4
            pluck(a_t, 0.22, a_freq, gain=0.22, pan=pan)

# Transition Risers
riser(6.8, 0.7, 300.0, 1400.0, gain=0.3)
riser(11.2, 0.8, 400.0, 2000.0, gain=0.35)
riser(15.2, 1.3, 250.0, 2800.0, gain=0.45)

# Scene 5: Outro Slam (16.5s to 20.0s)
cinematic_impact(16.5, gain=0.95)
for i in range(4):
    pluck(16.8 + i * 0.5, 1.4, [587.33, 698.46, 880.0, 1174.66][i], gain=0.28, pan=(i-1.5)*0.35)

# Master Limiter & Peak Normalization
max_peak = 0.0001
for i in range(NUM_SAMPLES):
    max_peak = max(max_peak, abs(left_channel[i]), abs(right_channel[i]))

norm_factor = 0.92 / max_peak
print(f"Peak level: {max_peak:.3f}, normalizing with factor {norm_factor:.3f}")

output_wav = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\brag-output\work\audio.wav"
with wave.open(output_wav, "wb") as wav_file:
    wav_file.setnchannels(2)
    wav_file.setsampwidth(2)
    wav_file.setframerate(SAMPLE_RATE)
    
    raw_bytes = bytearray()
    for i in range(NUM_SAMPLES):
        l = math.tanh(left_channel[i] * norm_factor)
        r = math.tanh(right_channel[i] * norm_factor)
        
        # 0.4s fade out at the very end
        fade_start = int((DURATION - 0.4) * SAMPLE_RATE)
        if i > fade_start:
            fade = (NUM_SAMPLES - i) / (NUM_SAMPLES - fade_start)
            l *= fade
            r *= fade
            
        int_l = int(clamp(l) * 32767.0)
        int_r = int(clamp(r) * 32767.0)
        raw_bytes.extend(struct.pack("<hh", int_l, int_r))
        
    wav_file.writeframes(raw_bytes)

print(f"Successfully generated {output_wav} ({DURATION}s, {SAMPLE_RATE}Hz stereo)")
