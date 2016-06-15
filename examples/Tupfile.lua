print('examples/Tupfile.lua')
table.insert(INCLUDES, '../reckless/include')
libreckless = '../reckless/lib/' .. LIBPREFIX .. 'reckless' .. LIBSUFFIX
for i, name in ipairs(tup.glob("*.cpp")) do
  obj = {compile(name)}
  table.insert(obj, libreckless)
  link(tup.base(name), obj)
end
