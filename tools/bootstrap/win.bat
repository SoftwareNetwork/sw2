@echo off

setlocal

cl -nologo -std:c++latest -EHsc -bigobj src/*.cpp -link -OUT:sw.exe
