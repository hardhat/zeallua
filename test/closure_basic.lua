local function make_adder(x)
    return function(y)
        return x + y
    end
end

local add_five = make_adder(5)
result = add_five(7)