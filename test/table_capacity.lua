local t = {1, 2, 3, 4, 5, 6, 7, 8}
t[9] = 99

if t[9] then
    x = 1
else
    x = t[8]
end