set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

echo "=== Compilando em modo Release ==="
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd "$DIR"

echo "=== Empacotando Distribuição Portátil Standalone ==="
DIST="dist/MIDI2Bridge-v5.1.0-Linux-x86_64"
rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/lib" "$DIST/plugins"

for b in build/MIDI2Bridge build/ump_oscilloscope build/ump_audio_sink; do
    if [ -f "$b" ]; then
        cp "$b" "$DIST/bin/"
        strip "$DIST/bin/$(basename "$b")" || true
    fi
done

python3 -c '
import os, sys, shutil, subprocess

dist = "dist/MIDI2Bridge-v5.1.0-Linux-x86_64"
system_blacklist = {
    "linux-vdso.so.1", "libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2",
    "librt.so.1", "ld-linux-x86-64.so.2", "libresolv.so.2"
}

copied = set()
def collect(bin_path):
    try:
        out = subprocess.check_output(["ldd", bin_path], text=True)
    except:
        return
    for line in out.splitlines():
        if "=>" in line:
            parts = line.strip().split("=>")
            libname = parts[0].strip()
            rest = parts[1].strip().split()[0]
            if rest.startswith("/") and os.path.exists(rest):
                if libname not in system_blacklist and not libname.startswith("libc.") and not libname.startswith("ld-linux"):
                    dest = f"{dist}/lib/{os.path.basename(rest)}"
                    if dest not in copied:
                        try:
                            shutil.copy2(rest, dest)
                            copied.add(dest)
                            soname = f"{dist}/lib/{libname}"
                            if not os.path.exists(soname):
                                os.symlink(os.path.basename(dest), soname)
                        except:
                            pass

for b in ["build/MIDI2Bridge", "build/ump_oscilloscope", "build/ump_audio_sink"]:
    collect(b)

qt_dirs = ["/usr/lib64/qt6/plugins", "/usr/lib/qt6/plugins"]
for d in qt_dirs:
    if os.path.exists(d):
        for cat in ["platforms", "imageformats", "iconengines", "tls"]:
            src_cat = os.path.join(d, cat)
            if os.path.exists(src_cat):
                shutil.copytree(src_cat, f"{dist}/plugins/{cat}", dirs_exist_ok=True)
                for root, _, files in os.walk(f"{dist}/plugins/{cat}"):
                    for f in files:
                        if f.endswith(".so"):
                            collect(os.path.join(root, f))
        break
'

cat << "EOF" > "$DIST/MIDI2Bridge.sh"
#!/bin/bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/MIDI2Bridge" "$@"
EOF

cat << "EOF" > "$DIST/ump_oscilloscope.sh"
#!/bin/bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/ump_oscilloscope" "$@"
EOF

cat << "EOF" > "$DIST/ump_audio_sink.sh"
#!/bin/bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/plugins"
exec "$HERE/bin/ump_audio_sink" "$@"
EOF

chmod +x "$DIST"/*.sh

echo "=== Compactando arquivos de Release ==="
tar -czf dist/MIDI2Bridge-v5.1.0-Linux-x86_64.tar.gz -C dist MIDI2Bridge-v5.1.0-Linux-x86_64
cd dist && zip -q -r MIDI2Bridge-v5.1.0-Linux-x86_64.zip MIDI2Bridge-v5.1.0-Linux-x86_64
cd "$DIR"

echo "=== Pacote Standalone v5.1.0 gerado com sucesso em dist/ ==="
