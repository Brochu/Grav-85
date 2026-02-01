@echo off

msbuild -nologo config.xml -t:%*
