function make_table()
    local t = {}
    t["same"] = 9
    return t
end

function read_table(t)
    return t["same"]
end

make_table = read_table(make_table())
read_table = 0