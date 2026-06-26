# Tocin — Linux download branch

This branch exists only to host a prebuilt download. It is **not** part of the
source history and can be deleted once you have the file.

## tocin-0.1.0-linux-x86_64.tar.gz

Self-contained Tocin compiler for Linux x86_64 (bundles LLVM 18, Boehm GC, etc.;
needs nothing beyond glibc).

```bash
tar xzf tocin-0.1.0-linux-x86_64.tar.gz
cd tocin-0.1.0-linux-x86_64
./install.sh
# open a new shell:
tocin --version
tocin file.to --run      # JIT, no external tools needed
```
