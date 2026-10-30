import os
import subprocess

Import("env")

pkg = env.subst("$PROJECT_DIR")
out = os.path.join(pkg, "src", "gen", "art.bin")
bake = os.path.join(pkg, "tools", "bake.py")

if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(bake):
    if subprocess.call(["uv", "run", "--quiet", "--with", "pillow", "python3", "-I", bake, pkg]) != 0:
        env.Exit(1)
