top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_cxx')
    ctx.env.append_value('INCLUDES', ['include'])
    ctx.env.append_value('CXXFLAGS', ['-std=c++11', '-Wall', '-pedantic', '-g', '-pthread', '-O0', '-march=native'])
    #ctx.env.append_value('CXXFLAGS', ['-fprofile-arcs', '-ftest-coverage'])
    ctx.env.append_value('LINKFLAGS', ['-g', '-pthread'])
    #ctx.env.append_value('LINKFLAGS', ['-fprofile-arcs'])
    ctx.env.append_value('DEFINES', ['NDEBUG'])
    ctx.env.append_value('LIB', ['rt'])

def build(ctx):
    # To see annotated assembly:
    # -std=c++11 -g -pthread -O3 -march=native -Wa,-adhln=main.s -masm=intel -fverbose-asm -c main.cpp
    ctx.stlib(source='src/asynclog.cpp src/output_buffer.cpp src/file_writer.cpp src/template_formatter.cpp src/input.cpp src/basic_log.cpp', target='asynclog')
    ctx.stlib(source='performance.cpp', target='performance')
    ctx.program(source='main.cpp', target='test',
            use='asynclog')
    ctx.program(source='measure_simple_call_burst.cpp', target='measure_simple_call_burst',
        use='asynclog performance')
    ctx.program(source='measure_periodic_calls.cpp', target='measure_periodic_calls',
        use='asynclog performance')
    ctx.program(source='measure_write_files.cpp', target='measure_write_files',
        use='asynclog performance')
