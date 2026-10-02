import os
import shutil
import hashlib

src_root = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject"
dst_root = r"C:\Users\sathi\OneDrive\Documents\NexVR Lab\NexVR Engine"

print(f"Syncing from:\n  {src_root}\nto:\n  {dst_root}\n")

def file_hash(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()

copied_count = 0
verified_count = 0

def sync_file(src, dst, label=None):
    global copied_count, verified_count
    if not os.path.exists(src):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst) or file_hash(src) != file_hash(dst):
        shutil.copy2(src, dst)
        copied_count += 1
        rel = os.path.relpath(dst, dst_root)
        print(f"[COPIED] {label or rel}")
    else:
        verified_count += 1

# 1. Header
sync_file(os.path.join(src_root, "include", "nexvr_sdk.h"), os.path.join(dst_root, "include", "nexvr_sdk.h"), "include/nexvr_sdk.h")

# 2. SDK Source
client_dir = "stereix-client" if os.path.exists(os.path.join(src_root, "stereix-client")) else "nexvr-client"
sdk_src = os.path.join(src_root, client_dir, "src", "sdk")
for root, dirs, files in os.walk(sdk_src):
    for f in files:
        s = os.path.join(root, f)
        rel = os.path.relpath(s, sdk_src)
        d = os.path.join(dst_root, "src", "sdk", rel)
        sync_file(s, d)

# 3. Adapters (Unreal and Unity)
adapters_src = os.path.join(src_root, client_dir, "src", "adapters")
for root, dirs, files in os.walk(adapters_src):
    for f in files:
        s = os.path.join(root, f)
        rel = os.path.relpath(s, adapters_src)
        d = os.path.join(dst_root, "src", "adapters", rel)
        sync_file(s, d)

# 4. Tests
tests_src = os.path.join(src_root, client_dir, "tests")
sdk_tests = [
    "test_sdk_validation.cpp",
    "test_sdk_imports.cpp",
    "test_sdk_dx12.cpp",
    "test_sdk_input.cpp",
    "test_unreal_bridge.cpp",
    "test_unity_bridge.cpp"
]
for t in sdk_tests:
    s = os.path.join(tests_src, t)
    d = os.path.join(dst_root, "tests", t)
    sync_file(s, d, f"tests/{t}")

# 5. Distribution (.zip)
dist_src = os.path.join(src_root, "dist")
if os.path.exists(dist_src):
    for root, dirs, files in os.walk(dist_src):
        for f in files:
            s = os.path.join(root, f)
            rel = os.path.relpath(s, dist_src)
            d = os.path.join(dst_root, "dist", rel)
            sync_file(s, d, f"dist/{rel}")

# 6. Scripts
scripts_src = os.path.join(src_root, "scripts")
for s_name in ["package_unreal_plugin.ps1", "sync_to_user_project.py"]:
    s = os.path.join(scripts_src, s_name)
    d = os.path.join(dst_root, "scripts", s_name)
    sync_file(s, d, f"scripts/{s_name}")

# 7. Documentation (.md in docs/)
docs_src = os.path.join(src_root, "docs")
for root, dirs, files in os.walk(docs_src):
    for f in files:
        if f.endswith(".md"):
            s = os.path.join(root, f)
            rel = os.path.relpath(s, docs_src)
            d = os.path.join(dst_root, "docs", rel)
            sync_file(s, d, f"docs/{rel}")

# 8. Root markdown files
for f in ["README.md", "AGENTS.md", "CONTRIBUTING.md", "SECURITY.md", "THIRD_PARTY_LICENSES.md"]:
    s = os.path.join(src_root, f)
    d = os.path.join(dst_root, f)
    sync_file(s, d, f)

print(f"\nSync Complete: {copied_count} files copied/updated, {verified_count} files up-to-date.")
