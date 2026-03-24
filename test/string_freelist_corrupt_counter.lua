-- Stress string allocator/free-list paths and assert corruption counter remains zero.
local i = 1
while i <= 120 do
    local a = "a" .. tostring(i)
    local b = "b" .. tostring(i * 11)
    local c = a .. "_" .. b .. "_" .. tostring(i)

    local t = {}
    t["x"] = a
    t["y"] = b
    t["z"] = c

    if i % 7 == 0 then
        local n = tostring(i * 17)
        t[n] = c .. n
    end

    i = i + 1
end

x = i
