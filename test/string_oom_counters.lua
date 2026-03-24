-- Attempt repeated string growth to force string allocator failures.
local i = 1
local s = ""
while i <= 200 do
    s = s .. "0123456789ABCDEF0123456789ABCDEF"
    i = i + 1
end

x = i
