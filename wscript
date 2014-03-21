top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_cxx')

def build(ctx):
    # To see annotated assembly:
    # -std=c++11 -g -pthread -O3 -march=native -Wa,-adhln=main.s -masm=intel -fverbose-asm -c dlog.cpp
    ctx.program(source='dlog.cpp main.cpp', target='test',
		cxxflags='-std=c++11 -g -pthread -O3 -march=native', linkflags='-g -pthread')
    ctx.program(source='dlog.cpp measure_simple_call_burst.cpp performance.cpp', target='measure_simple_call_burst',
        cxxflags='-std=c++11 -g -pthread -O3 -march=native', linkflags='-g -pthread')
    ctx.program(source='dlog.cpp measure_periodic_calls.cpp performance.cpp', target='measure_periodic_calls',
        cxxflags='-std=c++11 -g -pthread -O3 -march=native', linkflags='-g -pthread')
    #ctx.program(source='dlog.cpp main.cpp', target='test',
    #    cxxflags='-std=c++11 -g -pthread', linkflags='-g -pthread')
