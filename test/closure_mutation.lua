local function make_counter(start)
    local n = start

    return function()
        n = n + 1
        return n
    end
end

local counter = make_counter(10)
a = counter()
b = counter()