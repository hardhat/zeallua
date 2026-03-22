-- Factorial example for Tiny Lua

function factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

print("Factorial examples:")
print("5! =")
print(factorial(5))
print("6! =")
print(factorial(6))
print("7! =")
print(factorial(7))
