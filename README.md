# TinyCompiler

A tiny compiler written in modern C++ that transpiles Lisp to C++.

This is based on [James Kyle's excellent "The Super Tiny Compiler"](https://github.com/thejameskyle/the-super-tiny-compiler), which I discovered from [Jonathan Turner's really great programming languages and compilers reading list](http://www.jonathanturner.org/2016/10/programming-language-and-compilers-reading-list.html).

When working through James Kyle's code, I decided to try and implement the compiler in C++ to better understand it. It also gave me a chance to practice using modern C++11/14/17 features.


# How to build it

Install:
- CMake 3.6 or newer
- On Windows, Visual Studio 2015 Update 3 with Clang with Microsoft CodeGen (v140_clang_c2)

On Windows, run build_msvc.bat

On Linux (maybe Mac), run build_gcc.sh or build_clang.sh
