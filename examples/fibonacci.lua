-- Fibonacci sequence example for Tiny Lua

function fib(n)
    if n <= 1 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

print("Fibonacci sequence:")
for i = 0, 10 do
    print(fib(i))
end
