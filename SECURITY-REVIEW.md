# Security review notes — version 1.0.1

These notes describe the supplied C/Lua source and expected runtime behavior.
They are not an antivirus certification or a claim that Nexus has approved it.
The generic quarantine notice does not identify which security check was triggered.

## Release identification

- Archive: PBZAccessoryTypeUnlock_UE4SS_1.0.1.zip
- Archive size: 39327 bytes; no nested archives.
- Archive SHA256: 590be57a1016e1b51856832aeca934f0dcec00b8a821fe62a72f03ce3fae8c31
- Native module: native/PBZAccessoryRuntime.dll, 79490 bytes.
- Native module SHA256: d64471480609ecd2e934b1530a4224856d1917b71d66de1e31da1bc44f5923f6
- Runtime source SHA256: 16d646f9cdede6677da59332d54c393c660697ceb1cc5ba279427ce2eb54d191
- Lua source SHA256: d0b6f6ab5baa8c34759d7c21383d56b305b39dbb1fc06b222a5d60dca8f71840
- Build script SHA256: 288b806f9080eaa6cbadbd35134c8199c6f4d6dda103e58f38d453de3851bfe2

## Loading and native hooking

UE4SS runs Scripts/main.lua, which uses package.loadlib to load the companion DLL
and call luaopen_pbz_accessory_runtime inside the current game process.
The DLL hashes the on-disk main executable read-only using Windows BCrypt SHA256.
It accepts only this supported original executable:

4415933ce33476f14c837abb61a1b60af8b78cd81783896501913a7aac69e423

The runtime hook targets the inspected wrapped-script verification function at
RVA 0x062f65e0 of that exact build. MinHook is tried first. On MinHook allocation
failure, a fixed-build fallback checks the known 16-byte function prologue,
allocates a trampoline, changes its memory to executable/read-only and registers
Windows x64 unwind metadata with RtlAddFunctionTable. During fallback installation,
threads belonging to the current game process are briefly suspended and checked
before the 16-byte detour is written, then resumed. VirtualProtect is used to make
the target memory writable for the detour; instruction caches are flushed and
page protection is restored. The DLL is pinned until game exit, so hot unloading
is not supported. MinHook has its own thread/memory management as well.

Thus this mod DOES patch executable memory in the running game. Native DLL
loading, function detours and thread suspension may merit heuristic scrutiny.
They are disclosed rather than hidden. No packer/obfuscation is applied.

## Script changes

The hook calls the original verification function first and preserves its result.
Only after original verification succeeds does it consider these two paths:

- UI/Widget/Character_V2.js
- UI/Widget/PickupWeapon.js

It independently hashes the complete wrapped compiler input in UTF-8. Expected
SHA256 values are:

- Character_V2.js: 85073798f19c717f7ee8daa82f7264842250ebe24f5c08a8466e2c0d7e0b4c3d
- PickupWeapon.js: 8fcb49cffcce04b2c8857ad3b2745ea8643f379c216a3b0d6e5d9e0360c9015c

It checks that exactly one matching condition is present and the buffer is writable.
It changes the digit 3 to 6 in the corresponding UTF-16 compiler-input buffer:

```text
if (sameTypeCount >= 3)
newAccData.AccessoryType, accessorySlotIndex) >= 3)
```

Other scripts and mismatching source buffers are left untouched. Unsupported
executable hashes disable initialization. Script checksum records are not changed.
The two edits concern the equipment menu and pickup/equip path. Existing slots
and unrelated restrictions still apply.

## File/system access

The runtime opens the main executable read-only to hash it. Its only intended file
write is appending PBZAccessoryTypeUnlock.log inside its own mod directory.
The C/Lua source contains no network requests, downloads, subprocess launches,
registry access, credential collection, or writes to game executable/Pak/save files.
The hook modifies this game process, not another process.

This source review does not provide a VirusTotal result or prove the precise
reason Nexus quarantined the upload. Please inspect the source and binary and
make an independent moderation decision.

## 中文摘要

本 Mod 包含原生 DLL，会在当前游戏进程内安装函数钩子并修改可执行内存；
后备安装方式会短暂挂起当前游戏的其他线程。这些行为已如实公开，可能受到
启发式检测关注，但目前没有证据证明它们就是此次隔离的具体原因。
它只读校验磁盘 EXE，不写入 EXE/Pak/存档，不修改脚本校验表；仅修改两份
匹配完整哈希的编译输入中的阈值，并在自身目录写日志。以上不代表杀毒认证。
