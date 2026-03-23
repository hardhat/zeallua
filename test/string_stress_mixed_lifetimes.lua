-- Mixed short/medium/large-ish dynamic string workload to stress allocator reuse.
local i = 1
while i <= 28 do
    local short = "s" .. tostring(i)
    local medium = "mid_" .. short .. "_" .. short .. "_" .. tostring(i)
    local longish = medium .. "__" .. medium .. "__" .. short

    -- keep some strings alive briefly through a table, then drop
    local t = {}
    t["a"] = short
    t["b"] = medium
    t["c"] = longish

    if i % 4 == 0 then
        -- force some churn and conversions
        local x = tostring(i * 37)
        local y = "k" .. x .. "v" .. x
        t[y] = x
    end

    i = i + 1
end

x = i
