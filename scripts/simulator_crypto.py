"""Build real host mbedTLS for native simulators, without modifying firmware crypto."""
from pathlib import Path
import os
import shutil
import subprocess

Import("env")  # noqa: F821

root = Path(env["PROJECT_DIR"])
build = root / ".pio" / "host-crypto"
cmake = shutil.which("cmake")
if not cmake:
    candidate = Path(env.subst("$PROJECT_CORE_DIR")) / "packages/tool-cmake/bin" / (
        "cmake.exe" if os.name == "nt" else "cmake")
    if candidate.is_file():
        cmake = str(candidate)
if not cmake:
    raise RuntimeError("Native crypto requires CMake on PATH or PlatformIO's tool-cmake package")
subprocess.run([cmake, "-S", str(root / "scripts/simulator_crypto"), "-B", str(build),
                "-DCMAKE_BUILD_TYPE=Release"], check=True)
subprocess.run([cmake, "--build", str(build), "--config", "Release", "--target", "mbedcrypto",
                "--parallel", "4"], check=True)
# Prepend to avoid the simulator's incomplete sha256/base64 compatibility headers.
env.Prepend(CPPPATH=[str(build / "_deps/mbedtls-src/include")])
archives = list((build / "_deps/mbedtls-build/library").rglob("libmbedcrypto.a"))
if not archives:
    archives = list((build / "_deps/mbedtls-build/library").rglob("mbedcrypto.lib"))
if len(archives) != 1:
    raise RuntimeError("Expected exactly one native mbedcrypto static library")
env.Append(LIBS=[env.File(str(archives[0]))])
