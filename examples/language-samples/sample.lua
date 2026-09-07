-- Real Lua: metatables, closures, varargs, coroutines.
local M = {}

local function clamp(value, low, high)
    if value < low then return low end
    if value > high then return high end
    return value
end

local Cache = {}
Cache.__index = Cache

function Cache.new(capacity)
    return setmetatable({ capacity = capacity or 128, items = {}, count = 0 }, Cache)
end

function Cache:put(key, value)
    if self.items[key] == nil then
        self.count = self.count + 1
    end
    self.items[key] = value
end

function Cache:get(key, default)
    local found = self.items[key]
    if found ~= nil then return found end
    return default
end

M.producer = coroutine.create(function(...)
    for _, item in ipairs({...}) do
        coroutine.yield(item)
    end
end)

M.Cache, M.clamp = Cache, clamp
return M
