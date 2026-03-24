-- Second OOM scenario: repeated growth with occasional table traffic.
-- Validates alloc_fail_count and alloc_string_fail_count both increase.
local i = 1
local s = ""
while i <= 240 do
    s = s .. "ABCDEFGHIJKLMNOPQRSTUVWX0123456789"
    if i % 12 == 0 then
        local t = {}
        t["k"] = s
        t["n"] = tostring(i)
    end
    i = i + 1
end
x = i
