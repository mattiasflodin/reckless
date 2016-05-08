print('examples/Tupfile.lua')
table.insert(OPTIONS.includes, '../reckless/include')
libreckless = '../reckless/lib/' .. LIBPREFIX .. 'reckless' .. LIBSUFFIX
for i, name in ipairs(tup.glob("*.cpp")) do
  obj = {
    compile(name),
    libreckless
  }
  link(tup.base(name), obj)
end
