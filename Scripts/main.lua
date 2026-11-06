-- PBZAccessoryTypeUnlock UE4SS 1.0.1
-- Native companion transforms only two verified compiler-input conditions.
-- No Pak files, executable-file writes, or checksum-table changes are used.
local tag = "[PBZAccessoryTypeUnlock] "
local file = debug.getinfo(1, "S").source:gsub("^@", "")
local function parent(path) return path:gsub("[\\/][^\\/]*$", "") end
local folder = parent(parent(file))
local loader, why = package.loadlib(folder .. "/native/PBZAccessoryRuntime.dll", "luaopen_pbz_accessory_runtime")
if not loader then
    print(tag .. "Native component could not be loaded: " .. tostring(why) .. "\n")
    return
end
-- Keep this closure alive for the full Lua-mod lifetime. The native hook also
-- pins its DLL until process exit, so an accidental Lua reload stays safe.
_G.__PBZAccessoryRuntimeLoader = loader
local ok, err = pcall(loader)
if not ok then
    print(tag .. "Native component stopped: " .. tostring(err) .. "\n")
    return
end
print(tag .. "Runtime component initialized; see PBZAccessoryTypeUnlock.log for READY / APPLIED status.\n")

