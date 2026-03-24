-- Drive string-heavy allocations to pressure GC trigger counters.
local i = 1
while i <= 140 do
    local a = "left" .. tostring(i)
    local b = "right" .. tostring(i * 3)
    local c = a .. "::" .. b .. "::" .. tostring(i)

    local t = {}
    t["k1"] = a
    t["k2"] = b
    t["k3"] = c

    if i % 6 == 0 then
        local n = tostring(i * 29)
        t[n] = c .. n
    end

    i = i + 1
end

x = i
