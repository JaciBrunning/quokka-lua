x = 12
print(x)

-- function upvalueTest(a)
--   local i = a
--   local h = a
--   return function()
--     return i, h
--   end
-- end

-- table = {}
-- table["special"] = 34
-- print(table["spec" .. "ial"])

local table = {}
local table2 = {}
table.a = table2
table2.b = "hi"
print(table.a.b)