import lua

l = lua.Lua()
t = l.run('a = {3} return a')

t['abc'] = 'moo'
print(len(t), t.dict())
print(l.run('print(a["abc"])'))
setattr(t, 'foo', 3)
