x = 12
print(x)

function upvalueTest(a)
  local i = a
  local h = a
  return function()
    return i, h
  end
end