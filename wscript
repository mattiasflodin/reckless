top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_cxx')
    ctx.env.append_value('INCLUDES', ['boost'])
    ctx.env.append_value('CXXFLAGS', ['-std=c++11', '-g', '-pthread', '-O3', '-march=native'])
    ctx.env.append_value('LINKFLAGS', ['-g', '-pthread'])

def build(ctx):
    # To see annotated assembly:
    # -std=c++11 -g -pthread -O3 -march=native -Wa,-adhln=main.s -masm=intel -fverbose-asm -c dlog.cpp
    ctx.stlib(source='dlog.cpp', target='dlog')
    ctx.stlib(source='performance.cpp', target='performance')
    ctx.program(source='main.cpp', target='test',
            use='dlog')
    ctx.program(source='measure_simple_call_burst.cpp', target='measure_simple_call_burst',
        use='dlog performance')
    ctx.program(source='dlog.cpp measure_periodic_calls.cpp', target='measure_periodic_calls',
        use='dlog performance')
    ctx.program(source='dlog.cpp measure_write_files.cpp', target='measure_write_files',
        use='dlog performance')
