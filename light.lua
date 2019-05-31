function createAnonFunc()
  local i = 0
  return function()
    i = i + 1
    return i
  end
end

func = createAnonFunc();
test(func())
test(func())