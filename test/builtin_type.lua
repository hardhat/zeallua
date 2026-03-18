local kinds = { number = 7, ["function"] = 11 }

x = kinds[type(123)]
y = kinds[type(function() return 1 end)]