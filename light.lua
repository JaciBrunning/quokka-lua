start_time = os.clock()
function f()
  -- local a = 10 + 20
  -- local b = 10 / 20
  -- local c = "test" .. "thing"
  -- local d = { 1, 2, 3, 4, 5 }
  -- d[a] = b
  -- local e = d[a]
end

for i=1,1000 do
  -- f()
end
end_time = os.clock()

print(native_type(13.4))

print("Benchmark: " .. (end_time - start_time) .. " second(s)")