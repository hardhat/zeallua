-- Exercise mixed allocations so note_alloc_addr updates low/high watermarks.
local i = 1
while i <= 120 do
    local t = {}
    local a = "a" .. tostring(i)
    local b = "b" .. tostring(i * 9)
    local c = a .. "::" .. b
    t[a] = b
    t[b] = c
    if i % 8 == 0 then
        local x = tostring(i * 37)
        t[x] = c .. x
    end
    i = i + 1
end
x = i
