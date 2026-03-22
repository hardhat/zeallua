-- Tables example for Tiny Lua
-- Demonstrates both bracket and dot notation

-- Array-style table
numbers = {100, 200, 300, 400, 500}
print("Array elements:")
print(numbers[1])
print(numbers[3])
print(numbers[5])

-- String keys with dot notation
scores = {}
scores.alice = 95
scores.bob = 87
print("Scores:")
print(scores.alice)
print(scores.bob)

-- Update existing entry
scores.alice = 100
print("Updated alice:")
print(scores.alice)

-- Mixed table with both notations
t = {10, 20, 30}
t.name = 42
print("Mixed table:")
print(t[1])
print(t[2])
print(t.name)
