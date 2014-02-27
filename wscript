top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_cxx')

def build(ctx):
    ctx.program(source='dlog.cpp main.cpp',
            target='test', cxxflags='-std=c++11')
