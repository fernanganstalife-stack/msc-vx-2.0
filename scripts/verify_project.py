#!/usr/bin/env python3
"""Checks that every file referenced by CMakeLists.txt, the themes and the sources exists. Exit code 1 on problems."""
import glob, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
errors = []

def need(path, why):
    if not os.path.exists(os.path.join(ROOT, path)):
        errors.append(f"missing {path}  ({why})")

cmake = open(os.path.join(ROOT, "CMakeLists.txt")).read()
for m in re.finditer(r"^\s+((?:Source|tests)/[\w./]+)\s*\)?\s*$", cmake, re.M):
    need(m.group(1), "listed in CMakeLists.txt")
for f in ("README.md", ".github/workflows/build-windows-vst3.yml", "tests/dsp_test.cpp"):
    need(f, "required")

# every #include "x" must resolve next to the including file or in Source/
for src in glob.glob(os.path.join(ROOT, "Source", "**", "*.*"), recursive=True):
    if not src.endswith((".h", ".cpp")):
        continue
    for inc in re.findall(r'#include\s+"([^"]+)"', open(src).read()):
        here = os.path.join(os.path.dirname(src), inc)
        top = os.path.join(ROOT, "Source", inc)
        if not (os.path.exists(here) or os.path.exists(top)):
            errors.append(f"{os.path.relpath(src, ROOT)}: include \"{inc}\" not found")

# themes reference images
for t in glob.glob(os.path.join(ROOT, "Resources", "Themes", "*.json")):
    for key, name in json.load(open(t))["images"].items():
        need(os.path.join("Resources", "Images", name), f"{os.path.basename(t)} -> {key}")

# preset parameter ids must exist in Parameters.cpp
params = open(os.path.join(ROOT, "Source", "Parameters.h")).read()
known = set(re.findall(r'=\s*"([a-z_]+)"', params))
for p in glob.glob(os.path.join(ROOT, "Resources", "Presets", "*.json")):
    for pid in json.load(open(p))["params"]:
        if pid not in known:
            errors.append(f"{os.path.basename(p)}: unknown parameter id '{pid}'")

if errors:
    print("PROJECT CHECK FAILED"); [print(" -", e) for e in errors]; sys.exit(1)
print("Project check OK: all referenced files exist, all preset parameters are valid.")
