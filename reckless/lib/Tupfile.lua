sources = filter_platform_files(tup.glob("../src/*.cpp"))
objects = table.map(sources, function(name)
  return '../src/' .. tup.base(name) .. OBJSUFFIX
end)
library('reckless', objects)
