# Build instructions (Windows x64)

## Requirements

- PowerShell 7 or newer.
- A Windows x64 MinGW-w64 GCC toolchain with Windows SDK-compatible headers,
  bcrypt import library and static libgcc.
- Original release toolchain: GCC 16.1.0, MinGW-W64 x86_64-ucrt-posix-seh,
  built by Brecht Sanders, r1; target x86_64-w64-mingw32.
- MinHook v1.3.4 sources are included at third_party/minhook.
- UE4SS and PBZ are needed to use the finished mod, not to compile it.

Use an ASCII-only build directory (for example C:\PBZModReview). The original
MinGW linker had trouble with a Chinese output path. Extract this snapshot there
before building. The build output path must not already exist.

From PowerShell 7, in the extracted source directory:

```powershell
$ErrorActionPreference = 'Stop'
$compiler = 'C:\Toolchains\mingw64\bin\gcc.exe' # replace with your GCC path
$sourceRoot = (Get-Location).Path
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'GCC not found' }
& $compiler --version
if ($LASTEXITCODE -ne 0) { throw 'GCC failed' }
& $compiler -dumpmachine
if ($LASTEXITCODE -ne 0) { throw 'GCC failed' }
& (Join-Path $sourceRoot 'Source\build.ps1') -Gcc $compiler -MinHookSource (Join-Path $sourceRoot 'third_party\minhook') -Output (Join-Path $sourceRoot 'PBZAccessoryRuntime.dll')
```

The script invokes GCC with:

```text
-shared -O2 -std=c11 -Wall -Wextra -Werror -static-libgcc
-I third_party/minhook/include
Source/runtime.c
third_party/minhook/src/buffer.c
third_party/minhook/src/hook.c
third_party/minhook/src/trampoline.c
third_party/minhook/src/hde/hde64.c
-lbcrypt -Wl,--dynamicbase,--nxcompat -o PBZAccessoryRuntime.dll
```

PBZ_TEST_EXPORT is not defined for the release. The intended production export is
luaopen_pbz_accessory_runtime. No packer or obfuscation step is used.

## Installation layout after compilation

With the game closed, use this layout in the game's UE4SS Mods directory:

```text
PBZAccessoryTypeUnlock/
  enabled.txt
  Scripts/main.lua
  native/PBZAccessoryRuntime.dll
```

The mod also creates PBZAccessoryTypeUnlock.log in its own mod directory.
Do not combine it with the old Pak version. No changes to the on-disk PBZ.exe
are required. This snapshot does not include UE4SS or game files.

## Build equivalence

The original uploaded DLL SHA256 is recorded in SECURITY-REVIEW.md. The release
was linked under the filename PBZAccessoryRuntime.final.dll and then copied to
PBZAccessoryRuntime.dll. Compiler/linker versions, PE timestamps and output names
can affect binary bytes. These instructions build the supplied source; they do
not promise a bit-for-bit identical DLL. No deterministic-build claim is made.

## 中文提示

使用 PowerShell 7、64 位 MinGW-w64 GCC，先将本源码解压到纯英文路径。
修改示例中的编译器路径后执行命令。生成 DLL 后按上述目录放入 UE4SS Mods。
原发布包的源码已保持原样；不同工具链、时间戳和输出文件名可能导致 DLL 哈希不同。
