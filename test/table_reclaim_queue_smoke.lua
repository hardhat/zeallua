-- Verify discard paths stage table candidates for deferred reclaim.
-- The loop creates and discards 10 table locals; op_pop should enqueue each.
local i = 1
while i <= 10 do
    local t = {}
    t[1] = i
    i = i + 1
end
x = i
