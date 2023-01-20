#!/bin/bash

# using brew

export PATH="/usr/local/opt/gcc/bin:$PATH"
g++-12 -std=c++2b src/*.cpp -Isrc -I/usr/local/opt/fmt/include -o sw
