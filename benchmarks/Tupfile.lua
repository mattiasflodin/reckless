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

  if tup.getconfig('TRACE_LOG') != '' and lib == 'reckless' then
    table.insert(OPTIONS.define, 'RECKLESS_ENABLE_TRACE_LOG')
  end

  single_threaded('periodic_calls')
  single_threaded('write_files')

  mandelbrot_obj = compile('mandelbrot.cpp', 'mandelbrot' .. '-' .. lib .. OBJSUFFIX)
  for threads=1,8 do
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

link('nanolog_benchmark', {
  compile('nanolog_benchmark.cpp', 'nanolog_benchmark' .. OBJSUFFIX),
  libreckless
})
pop_options()

SPDLOG = tup.getconfig('SPDLOG')
if SPDLOG ~= '' then
  push_options()
  table.insert(OPTIONS.includes, joinpath(SPDLOG, 'include'))
  build_suite('spdlog', {})
  pop_options()
end

G3LOG_INCLUDE = tup.getconfig('G3LOG_INCLUDE')
G3LOG_LIB = tup.getconfig('G3LOG_LIB')
if G3LOG_INCLUDE ~= '' and G3LOG_LIB ~= '' then
  push_options()
  table.insert(OPTIONS.includes, G3LOG_INCLUDE)
  table.insert(OPTIONS.libdirs, G3LOG_LIB)
  table.insert(OPTIONS.libs, 'g3logger')
  build_suite('g3log', {})
  pop_options()
end

BOOST_LOG_INCLUDE = tup.getconfig('BOOST_LOG_INCLUDE')
BOOST_LOG_LIB = tup.getconfig('BOOST_LOG_LIB')
if BOOST_LOG_INCLUDE ~= '' and BOOST_LOG_LIB ~= '' then
  push_options()
  libs = {
    'boost_log',
    'boost_log_setup',
    'boost_system',
    'boost_thread'
  }

  table.insert(OPTIONS.define, 'BOOST_ALL_DYN_LINK')
  table.insert(OPTIONS.includes, BOOST_LOG_INCLUDE)
  table.insert(OPTIONS.libdirs, BOOST_LOG_LIB)
  OPTIONS.libs = table.merge(OPTIONS.libs, libs)
  build_suite('boost_log', {})
end
