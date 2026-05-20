# Mayya


A minimal low-level container runtime written in C++ using native Linux kernel features such as namespaces, cgroups, and OverlayFS.

Mayya implements containerization primitives directly instead of relying on existing container runtimes or container orchestration libraries


## Build
```
cmake -B build
cmake --build build
```

## Usage
```
./mayya run /bin/sh [args]
```

NOTE: When using `/bin/bash` host shell prompt and container shell prompt can look identical. To fix this use `sudo` or `/bin/sh`.
