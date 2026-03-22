-- Exercise table allocator pressure to drive soft/force GC trigger counters.
local i = 1
while i <= 320 do
    local t = {}
    t[1] = i
    i = i + 1
end
x = i
