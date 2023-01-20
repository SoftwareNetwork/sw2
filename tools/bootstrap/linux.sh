#!/bin/bash

g++ -std=c++2b src/*.cpp -o sw

# some other variants
# g++ src/*.cpp -std=c++2b -O3 -flto -fomit-frame-pointer -static -fvisibility=hidden -o sw
# strip -s sw
