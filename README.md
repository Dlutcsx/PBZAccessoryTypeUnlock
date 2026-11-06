# PBZAccessoryTypeUnlock UE4SS 1.0.1 — review source

This source snapshot is provided for review of the Nexus Mods release named
PBZAccessoryTypeUnlock UE4SS 1.0.1. It contains no compiled DLL or nested archives.
Source/runtime.c, Source/build.ps1 and Scripts/main.lua are copied byte-for-byte
from the release package. See SHA256SUMS.txt for their hashes.

The mod changes the same-category accessory threshold from 3 to 6 in two verified
script compiler-input buffers. It still uses the game's available accessory slots.
It requires UE4SS and the exact supported original PBZ.exe listed in SECURITY-REVIEW.md.
The author/user confirmed that a fourth same-category accessory can be equipped.

Read BUILD.md for Windows x64 compilation and SECURITY-REVIEW.md for runtime
hooking behavior and expected file access. No disk executable patch or separate
launcher is required. The mod does modify executable memory inside the running game.

Third-party dependency: MinHook v1.3.4, commit
c3fcafdc10146beb5919319d0683e44e3c30d537, vendored from
https://github.com/TsudaKageyu/minhook . Its license and authors are retained under
third_party/minhook. This snapshot does not assign a new license to the mod's own code.

## 中文说明

本目录用于公开源码和提交 Nexus 人工审核，不是替换原上传文件的游戏安装包。
自有 C、Lua 源码及构建脚本与 1.0.1 发布包逐字节一致；MinHook 依赖源码已附带。
功能是将两处同类饰品数量判断从 3 改为 6，仍受游戏现有装备栏位约束。
它不写入磁盘上的 PBZ.exe，但会在游戏进程内安装函数钩子、修改内存。
构建方法见 BUILD.md，详细行为见 SECURITY-REVIEW.md。
