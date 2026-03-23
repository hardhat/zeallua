-- Drive table GC and capture adjacent string + trigger counters.
-- This baseline ensures string mark/sweep counters are reported alongside
-- trigger/cycle telemetry during a deterministic allocation workload.
local i = 1
while i <= 320 do
    local t = {}
    t[1] = i
    i = i + 1
end

x = i
