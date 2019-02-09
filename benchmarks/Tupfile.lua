libreckless = '../reckless/lib/' .. LIBPREFIX .. 'reckless' .. LIBSUFFIX
libperformance_log = '../performance_log/lib/' .. LIBPREFIX .. 'performance_log' .. LIBSUFFIX

table.insert(OPTIONS.includes, joinpath(tup.getcwd(), '../performance_log/include'))

function build_suite(lib, extra_objs)
  push_options()
  table.insert(OPTIONS.define, 'LOG_INCLUDE=\\"' .. lib .. '.hpp\\"')

  function single_threaded(name)
    local objs = {
      compile(name .. '.cpp', name .. '-' .. lib .. OBJSUFFIX),
      libperformance_log
    }
    objs = table.merge(objs, extra_objs)
    link(name .. '-' .. lib, objs)
  end

  single_threaded('periodic_calls')
  single_threaded('write_files')

  if tup.getconfig('TRACE_LOG') != '' and lib == 'reckless' then
    table.insert(OPTIONS.define, 'RECKLESS_ENABLE_TRACE_LOG')
  end
  mandelbrot_obj = compile('mandelbrot.cpp', 'mandelbrot' .. '-' .. lib .. OBJSUFFIX)
  for threads=1,4 do
    push_options()
    table.insert(OPTIONS.define, 'THREADS=' .. threads)
    local exesuffix = '-' .. lib .. '-' .. threads
    local objsuffix = exesuffix .. OBJSUFFIX

    -- call_burst
    local objs = {
      compile('call_burst.cpp', 'call_burst' .. objsuffix),
      libperformance_log
    }
    objs = table.merge(objs, extra_objs)
    link('call_burst' .. exesuffix, objs)

    -- mandelbrot
    objs = {
      compile('benchmark_mandelbrot.cpp', 'benchmark_mandelbrot' .. objsuffix),
      mandelbrot_obj
    }
    objs = table.merge(objs, extra_objs)
    link('mandelbrot' .. exesuffix, objs)
    pop_options()
  end
  pop_options()
end

build_suite('nop', {})
build_suite('stdio', {})
build_suite('fstream', {})

push_options()
table.insert(OPTIONS.includes, tup.getcwd() .. '/../reckless/include')
build_suite('reckless', {libreckless}, {}, {}, {})
pop_options()

SPDLOG = tup.getconfig('SPDLOG')
if SPDLOG ~= '' then
  push_options()
  table.insert(OPTIONS.includes, joinpath(SPDLOG, 'include'))
  build_suite('spdlog', {})
  pop_options()
end

PANTHEIOS = tup.getconfig('PANTHEIOS')
if PANTHEIOS ~= '' then
  STLSOFT = tup.getconfig('STLSOFT')
  if STLSOFT == '' then
    error('CONFIG_PANTHEIOS requires CONFIG_STLSOFT')
  end
  PANTHEIOS_COMPILER = tup.getconfig('PANTHEIOS_COMPILER')
  if PANTHEIOS_COMPILER == '' then
      error('CONFIG_PANTHEIOS requires CONFIG_PANTHEIOS_COMPILER')
  end

  push_options()
  local pcc = PANTHEIOS_COMPILER
  libs = {
    'pantheios.1.core.' .. pcc .. '.mt',
    'pantheios.1.util.' .. pcc .. '.mt',
    'pantheios.1.fe.simple.' .. pcc .. '.mt',
    'pantheios.1.be.file.' .. pcc .. '.mt',
    'pantheios.1.bec.file.' .. pcc .. '.mt',
    'pantheios.1.util.' .. pcc .. '.mt'
  }
  OPTIONS.libs = table.merge(OPTIONS.libs, libs)
  table.insert(OPTIONS.libdirs, joinpath(PANTHEIOS, 'lib'))
  table.insert(OPTIONS.includes, joinpath(PANTHEIOS, 'include'))
  table.insert(OPTIONS.includes, joinpath(STLSOFT, 'include'))

  if string.find(CXX, 'g++') then
    -- This warning comes up a *lot* on pantheios for my version of g++,
    -- ofuscating everything else. It's not supported by clang though, so
    -- we need to be judicious about its use to avoid failed builds. I
    -- suppose it would be best if we could test for its availability, or
    -- better yet if pantheios would fix its code.
    table.insert(OPTIONS.cflags, '-Wno-unused-local-typedefs')
  end
  build_suite('pantheios', {})
  pop_options()
end
