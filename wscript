top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_cxx')
    ctx.env.append_value('INCLUDES', ['boost'])
    ctx.env.append_value('CXXFLAGS', ['-std=c++11', '-Wall', '-pedantic', '-g', '-pthread', '-O3', '-march=native'])
    #ctx.env.append_value('CXXFLAGS', ['-fprofile-arcs', '-ftest-coverage'])
    ctx.env.append_value('LINKFLAGS', ['-g', '-pthread'])
    #ctx.env.append_value('LINKFLAGS', ['-fprofile-arcs'])
    #ctx.env.append_value('DEFINES', ['NDEBUG'])
    ctx.env.append_value('LIB', ['rt'])

def build(ctx):
    ctx.recurse('asynclog')
    ctx.recurse('performance_log')
    ctx.recurse('performance')
