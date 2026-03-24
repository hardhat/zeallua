-- Drive sustained table allocation pressure; hysteresis latches should avoid counter spam.
local i = 1
while i <= 360 do
    local t = {}
    t[1] = i
    t[2] = i + 1
    i = i + 1
end

x = i
