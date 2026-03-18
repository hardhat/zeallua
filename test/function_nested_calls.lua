function inc(n)
    return n + 1
end

function plus_two(n)
    return inc(inc(n))
end

plus_two = plus_two(4)
inc = 0