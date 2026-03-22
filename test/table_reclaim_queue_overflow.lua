-- Queue more discarded table values than the ring can hold.
-- Capacity is 64 slots with one slot reserved to distinguish full/empty,
-- so at most 63 pending entries are kept and the rest increment drop count.
local i = 1
while i <= 100 do
    local t = {}
    t[1] = i
    i = i + 1
end
x = i
