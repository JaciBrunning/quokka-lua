x = {}
x[3.14] = 12
b = x
b[6.28] = "AA"
print(x[6.28])
print(myfunc(12, 5))

function myfunc(a, b)
  return a + b
end

function upvalueTest()
  local i = 0
  return function()
    i = i + 1
    return i
  end
end