-- Keep one table rooted in globals and stage many dropped table candidates.
-- With gc_sweep_enabled defaulting to 0 this test only validates baseline output.
root = {}
local i = 1
while i <= 20 do
    local t = {}
    t[1] = i
    i = i + 1
end
x = i
