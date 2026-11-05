#!/usr/bin/env python3
"""Generates Resources/Presets/*.json and Resources/Themes/*.json (already committed; run to regenerate)."""
import colorsys, json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------- presets
# Values are in REAL units (dB, %, ms ...). Choices use their display name.
BASE = {
 "in_gain": 0.0, "out_gain": 0.0, "master_mix": 100.0,
 "at_on": True, "at_scale": "Minor", "at_note": "C", "at_amount": 100.0, "at_speed": 20.0, "at_mode": "Robotic",
 "cp_on": True, "cp_thresh": -12.0, "cp_ratio": 4.0, "cp_release": 120.0, "cp_makeup": 4.0,
 "ds_on": True, "ds_thresh": -30.0, "ds_freq": 6000.0, "ds_reduction": -12.0, "ds_amount": 70.0,
 "eq_on": True, "eq_low": -3.0, "eq_mid": 1.5, "eq_high": 2.0,
 "air_on": True, "air_amount": 80.0,
 "di_on": True, "di_drive": 30.0, "di_tone": 50.0, "di_mix": 50.0,
 "sa_on": True, "sa_drive": 35.0, "sa_mix": 40.0, "sa_type": "Tube",
 "rv_on": True, "rv_size": 50.0, "rv_decay": 2.8, "rv_damping": 50.0, "rv_predelay": 20.0, "rv_mix": 25.0, "rv_type": "Hall",
 "dl_on": True, "dl_time": 350.0, "dl_feedback": 35.0, "dl_mix": 40.0, "dl_tone": 60.0, "dl_sync": True,
 "dl_div": "1/4", "dl_dotted": False, "dl_triplet": False, "dl_stereo": True,
 "ch_on": True, "ch_rate": 1.2, "ch_depth": 70.0, "ch_width": 100.0, "ch_mix": 60.0,
 "sw_on": True, "sw_width": 120.0, "sw_mix": 100.0,
}

PRESETS = [
 ("01 - MySpace Dream", "Dreamy pop-punk lead: soft retune, airy top, long chorus and delay.",
  {"at_scale": "Major", "at_mode": "Natural", "at_speed": 45.0, "at_amount": 85.0, "di_on": False, "sa_drive": 25.0,
   "rv_mix": 32.0, "rv_decay": 3.4, "dl_mix": 34.0, "dl_div": "1/8", "dl_dotted": True, "ch_mix": 55.0, "air_amount": 85.0}),
 ("02 - Destroyed Dream", "Crushed, grungy double-tracker with heavy saturation and robotic tuning.",
  {"at_mode": "Hard", "at_speed": 5.0, "di_on": True, "di_drive": 62.0, "di_mix": 60.0, "sa_drive": 60.0, "sa_mix": 55.0,
   "sa_type": "Transistor", "eq_low": -5.0, "eq_high": 3.0, "rv_mix": 28.0, "rv_type": "Plate", "dl_mix": 30.0, "ch_mix": 40.0}),
 ("03 - Chorus Hell", "Huge stacked chorus section: wide, shimmering, very present.",
  {"at_mode": "Smooth", "at_speed": 60.0, "ch_mix": 85.0, "ch_depth": 90.0, "ch_rate": 0.8, "sw_width": 170.0, "rv_mix": 38.0,
   "rv_size": 75.0, "rv_decay": 4.2, "dl_mix": 45.0, "dl_div": "1/4", "dl_dotted": True, "cp_ratio": 5.0, "air_amount": 90.0}),
 ("04 - Vampire Love", "Dark minor-key croon: warm tube saturation, plate reverb, ping-pong echoes.",
  {"at_scale": "Harmonic Minor", "at_note": "A", "at_mode": "Natural", "at_speed": 35.0, "eq_low": 1.0, "eq_high": 0.5,
   "air_amount": 55.0, "sa_drive": 45.0, "sa_mix": 55.0, "rv_type": "Plate", "rv_mix": 35.0, "rv_decay": 3.8, "dl_mix": 38.0,
   "dl_div": "1/8", "ch_mix": 35.0, "di_on": False}),
 ("05 - Angel Voice", "Pure and floating: gentle correction, big air, cathedral hall.",
  {"at_scale": "Major", "at_note": "C", "at_mode": "Smooth", "at_speed": 80.0, "at_amount": 70.0, "air_amount": 100.0,
   "eq_high": 3.5, "eq_low": -4.0, "cp_ratio": 3.0, "sa_drive": 15.0, "sa_mix": 25.0, "rv_size": 90.0, "rv_decay": 6.5,
   "rv_mix": 45.0, "rv_damping": 35.0, "dl_mix": 28.0, "dl_div": "1/4", "dl_dotted": True, "ch_mix": 45.0, "sw_width": 150.0, "di_on": False}),
 ("06 - Everything Broken", "Glitchy-sad vocal: hard retune, lo-fi tape, short stereo slapback.",
  {"at_mode": "Hard", "at_speed": 0.0, "at_scale": "Pentatonic Min", "sa_type": "Tape", "sa_drive": 55.0, "sa_mix": 60.0,
   "di_on": True, "di_drive": 40.0, "di_mix": 35.0, "eq_high": -2.0, "air_amount": 40.0, "rv_type": "Room", "rv_mix": 22.0,
   "rv_decay": 1.4, "dl_div": "1/16", "dl_mix": 42.0, "dl_feedback": 45.0, "ch_mix": 30.0, "sw_width": 90.0}),
 ("07 - Raw Underground", "Dry, aggressive garage-punk shout. Minimal space, tight compression.",
  {"at_amount": 40.0, "at_mode": "Natural", "at_speed": 120.0, "cp_thresh": -18.0, "cp_ratio": 6.0, "cp_makeup": 7.0,
   "di_on": True, "di_drive": 55.0, "di_mix": 45.0, "sa_drive": 50.0, "sa_type": "Transistor", "eq_low": -6.0, "eq_mid": 3.0,
   "air_amount": 35.0, "rv_mix": 8.0, "rv_type": "Room", "rv_decay": 0.9, "dl_mix": 12.0, "dl_div": "1/16", "ch_on": False, "sw_width": 100.0}),
 ("08 - MySpaceCore", "The reference sound of the MSC VOX 2.0 artwork: C minor, robotic tune, bright and wide.", {}),
]

def build_presets():
    outdir = os.path.join(ROOT, "Resources", "Presets")
    os.makedirs(outdir, exist_ok=True)
    for i, (name, desc, over) in enumerate(PRESETS, 1):
        params = dict(BASE); params.update(over)
        slug = name.split(" - ")[1].lower().replace(" ", "_")
        with open(os.path.join(outdir, f"preset_{i:02d}_{slug}.json"), "w") as f:
            json.dump({"name": name, "description": desc, "params": params}, f, indent=2)

# ---------------------------------------------------------------- themes
def hexcol(h, s, v):
    r, g, b = colorsys.hsv_to_rgb((h % 360) / 360.0, s, v)
    return "#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255))

THEMES = [
 ("01_purple", "Purple Punk", 0), ("02_blue", "Midnight Blue", -60), ("03_green", "Toxic Green", -135),
 ("04_pink", "Hot Pink", 55), ("05_gold", "Gold Rush", 125), ("06_cyan", "Cyber Cyan", -95),
]
def build_themes():
    outdir = os.path.join(ROOT, "Resources", "Themes")
    os.makedirs(outdir, exist_ok=True)
    for suffix, name, deg in THEMES:
        h0 = 262 + deg
        t = {
            "name": name,
            "background": hexcol(h0, 0.80, 0.07),
            "panel": hexcol(h0, 0.75, 0.15),
            "panelBorder": hexcol(h0, 0.70, 0.62),
            "primary": hexcol(h0, 0.62, 0.96),
            "accent": hexcol(h0 + 10, 0.30, 1.0),
            "highlight": hexcol(h0 + 55, 0.55, 1.0),
            "text": hexcol(h0, 0.12, 1.0),
            "subtext": hexcol(h0, 0.35, 0.85),
            "knobBody": hexcol(h0, 0.55, 0.32),
            "images": {
                "banner": f"banner_{suffix}.jpg", "singer": f"singer_{suffix}.png",
                "guitarist": f"guitarist_{suffix}.png", "deco": f"deco_{suffix}.jpg", "title": f"title_{suffix}.jpg",
            },
        }
        with open(os.path.join(outdir, f"theme_{suffix}.json"), "w") as f:
            json.dump(t, f, indent=2)

if __name__ == "__main__":
    build_presets(); build_themes(); print("ok")
