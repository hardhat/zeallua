local function outer(a)
    local b = a + 1

    local function middle(c)
        return function(d)
            return b + c + d
        end
    end

    return middle(10)
end

result = outer(1)(5)