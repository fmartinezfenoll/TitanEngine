# TitanEngine

Modular C++ 3D engine base built on **OpenGL 4.5**, designed for performance, clarity and extensibility.

---

## Overview

TitanEngine is a custom graphics engine written in modern C++ focused on:

- Clean modular architecture  
- Cross-platform build system (Windows / Linux)  
- Debug and Release configurations  
- High-performance OpenGL 4.5 rendering backend  
- Scalable project structure  

The engine is designed as a foundation for advanced rendering features and real-time applications.

---

## Features

- OpenGL 4.5 Core Profile
- Modular engine structure
- Custom Makefile build system
- Debug / Release configurations
- Static linking support (Release)
- Cross-platform support (Windows / Linux)
- Clear separation between engine and application

---


## Build System

The project uses a custom Makefile supporting multiple configurations.

### Debug Build
```bash
make CONFIG=debug

Requirements

C++17 compatible compiler (GCC / MinGW / Clang)

OpenGL 4.5 compatible GPU

Make (Linux or MinGW on Windows)