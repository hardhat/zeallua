-- Force low-free conditions while exercising string-heavy payloads.
-- Expect gc_trigger_force_count to become non-zero under pressure.
local i = 1
while i <= 420 do
    local t = {}
    local s1 = "k" .. tostring(i)
    local s2 = "v" .. tostring(i * 13)
    local s3 = s1 .. "::" .. s2 .. "::" .. tostring(i)
    t[s1] = s2
    t[s2] = s3
    t[s3] = s1
    i = i + 1
end
x = i
