sources = tup.glob("../src/*.cpp")
objects = table.map(sources, function(name)
  return '../src/' .. tup.base(name) .. OBJSUFFIX
end)
library('performance_log', objects)

