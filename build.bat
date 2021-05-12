@echo off
call cl -nologo -O2 largepages.cpp advapi32.lib
REM call clang++ -O2 -fuse-ld=lld -Wl,-subsystem:console,advapi32.lib largepages.cpp -o largepages.exe
