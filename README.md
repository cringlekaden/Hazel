# Hazel

[![C/C++ CI](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml/badge.svg?branch=master)](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml)

Following TheCherno's Hazel Game Engine Series, but adding complete Linux support while keeping complete Windows support.

## Build on Linux

Install GCC, GNU Make, a recent development build of Premake 5, and the
development headers for OpenGL and X11. Clone the pinned dependencies and build:

```sh
git clone --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
premake5 gmake
make config=debug -j2
```
