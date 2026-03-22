-- Loop test for Z80 compilation
-- Compute factorial iteratively

local n = 5
local result = 1

while n > 1 do
    result = result * n
    n = n - 1
end

print(result)

-- For loop test: sum 1 to 10
local sum = 0
for i = 1, 10 do
    sum = sum + i
end
print(sum)
