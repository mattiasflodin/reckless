function shallowcopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in pairs(orig) do
            copy[orig_key] = orig_value
        end
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

function deepcopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
        end
        setmetatable(copy, deepcopy(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

function string.startswith(s, suffix)
  return suffix=='' or string.sub(s, 1, string.len(suffix))==suffix
end

function string.endswith(s, suffix)
  return suffix=='' or string.sub(s, -string.len(suffix))==suffix
end

function table.merge(t1, t2)
  t = shallowcopy(t1)
  for _, v in ipairs(t2) do
    table.insert(t, v)
  end
  return t
end

function table.map(t, f)
  local result = {}
  for k, v in pairs(t) do
    result[k] = f(v)
  end
  return result
end

function table.filter(t, f)
  local result = {}
  local i = 1
  for k, v in pairs(t) do
    if f(k, v) then
      result[i] = v
      i = i+1
    end
  end
  return result
end

function join(sep, ...)
 local result = ''
 for _, segment in ipairs({...}) do
   if result ~= '' then
     result = result .. sep .. segment
   else
     result = segment
   end
 end
 return result
end

function table.flatten(t)
  local nt = {}
  for _, v in ipairs(t) do
    if type(v) == 'table' then
      local t2 = table.flatten(v)
      for _, v in ipairs(t2) do
        table.insert(nt, v)
      end
    else
      table.insert(nt, v)
    end
  end
  return nt
end

function joinargs(...)
  return table.concat(table.flatten({...}), ' ')
end

function joinpath(...)
 local result = ''
 for i, segment in ipairs({...}) do
   if segment:startswith('/') then
     result = segment
   else
     if result ~= '' and not result:endswith('/') then
       result = result .. '/'
     end
     result = result .. segment
   end
 end
 return result
end

OPTIONS = {
  define = {},
  includes = {},
  libs = {},
  libdirs = {},
  cflags = {}
}

OPTION_STACK = {}

function push_options()
  table.insert(OPTION_STACK, OPTIONS)
  OPTIONS = deepcopy(OPTIONS)
end

function pop_options()
  OPTIONS = table.remove(OPTION_STACK)
end

CXX = tup.getconfig('CXX')
LD = tup.getconfig('LD')
AR = tup.getconfig('AR')
DEBUG = tup.getconfig('DEBUG') == 'yes'
PLATFORM = tup.getconfig('TUP_PLATFORM')
if CXX == '' then
  if PLATFORM == 'linux' then
    CXX = 'g++'
  elseif PLATFORM == 'win32' then
    CXX = 'cl'
  end
end

if PLATFORM == 'linux' then
  if CXX == '' then
    CXX = 'g++'
  end
  if LD == '' then
    LD = 'g++'
  end
  if AR == '' then
    AR = 'ar'
  end
  if DEBUG then OPTIMIZATION = '-O0' else OPTIMIZATION = '-O3'
  end
  
  LDFLAGS = '-g'
  ARFLAGS = 'crs'
  
  COMPILER_COMPILE_ONLY_FLAG = '-c'
  COMPILER_OUTPUT_FLAG = '-o'
  COMPILER_DEFINE_FLAG = '-D'
  COMPILER_INCLUDE_FLAG = '-I'
  LINKER_OUTPUT_FLAG = '-o'
  LINKER_LIBDIR_FLAG = '-L'
  LINKER_LIB_FLAG = '-l'
  
  OBJSUFFIX = '.o'
  EXESUFFIX = ''
  LIBPREFIX = 'lib'
  LIBSUFFIX = '.a'
  
  OPTIONS.cflags = {'-ggdb3', '-std=c++11', '-Wall', '-Werror'}
  
elseif PLATFORM == 'win32' then
  if CXX == '' then
    CXX = 'cl'
  end
  if LD == '' then
    LD = 'link'
  end
  if DEBUG then OPTIMIZATION = '/Od /Oi /MDd' else OPTIMIZATION = '/O2 /GL /MD'
  end

  OBJSUFFIX = '.obj'
  options.cflags = {
    '/EHsc', -- C++ exception handling
    '/Zc:forScope /Zc:wchar_t /Zc:auto /Zc:rvalueCast /Zc:strictStrings /Zc:implicitNoexcept /Zc:inline /Zc:throwingNew /Zc:referenceBinding', -- C++11 conformance
    '/fp:precise', -- Predictable floating-point operations
    '/W3',         -- Warning level 3
    '/WX',         -- warnings as errors
    '/GS', '/sdl', -- Security checks
    '/Zi',         -- Generate debug PDB
    '/Gm-',        -- Disable minimal rebuild
    '/FS',         -- Force serialized access to PDB file
    '/nologo',
    '/Fd' .. joinpath(tup.getcwd(), 'reckless.pdb')
  }
  LDFLAGS = ''
  ARFLAGS = '/NOLOGO'

  COMPILER_COMPILE_ONLY_FLAG = '/c'
  COMPILER_OUTPUT_FLAG = '/Fo:'
  COMPILER_DEFINE_FLAG = '/D'
  COMPILER_INCLUDE_FLAG = '/I'
  LINKER_OUTPUT_FLAG = '/OUT:'
  LINKER_LIBDIR_FLAG = '/LIBPATH:'
  LINKER_LIB_FLAG = ''
  
  EXESUFFIX = '.exe'
  LIBPREFIX = ''
  LIBSUFFIX = '.lib'
  error("build scripts are unfinished on VC++ due to tup issue #288")
else
  error("build scripts not implemented for this platform")
end

--function file_exists(path)
--  local f = io.open(path, 'r')
--  if f~=nil then io.close(f) return true else return false end
--end
--
--function which_file(paths, name)
--  for i, path in ipairs(paths) do
--    local fp = join(path, name)
--    if file_exists(fp) then
--      print('exists', fp)
--      return fp
--    end
--  end
--  print('not exists')
--  return nil
--end

function filter_platform_files(files)
  local platform = tup.getconfig('TUP_PLATFORM')
  local result = {}
  if platform == 'win32' then
    return table.filter(files, function(k, v)
      local name = tup.base(v)
      return not string.endswith(name, '_unix')
    end)
  elseif  platform == 'linux' then
    return table.filter(files, function(k, v)
      local name = tup.base(v)
      return not string.endswith(name, '_win32')
    end)
  end
  return result
end

function array_to_option_string(array, option)
  local args = {}
  for i, v in ipairs(array) do
    if v:find(' ') then v = ' "' .. v .. '"' end
    args[i] = option .. v
  end
  return table.concat(args, ' ')
end

function compile(name, objname, options)
  if options == nil then
    options = OPTIONS
  end
  
  -- Include directories
  include_args = array_to_option_string(options.includes, COMPILER_INCLUDE_FLAG)
  define_args = array_to_option_string(options.define, COMPILER_DEFINE_FLAG)
  
  -- Object file name
  local base = tup.base(name)
  if objname == nil then
    objname = tup.base(name) .. OBJSUFFIX
  end

  local cmd = joinargs(CXX, COMPILER_COMPILE_ONLY_FLAG, options.cflags,
    OPTIMIZATION, include_args, define_args, COMPILER_OUTPUT_FLAG .. objname,
    name)
  local text = '^ CXX ' .. name .. ' -> ' .. objname ..  '^ '
  tup.definerule{inputs={name}, command=text .. cmd, outputs={objname}}
  return objname
end

function library(name, objects)
  name = LIBPREFIX .. name .. LIBSUFFIX
  local cmd = joinargs(AR, ARFLAGS, name, objects)
  
  text = '^ AR ' .. name .. '^ '
  tup.definerule{inputs=objects, command=text .. cmd, outputs={name}}
end

function link(name, objects, options)
  if options == nil then
    options = OPTIONS
  end
  
  libdirs = array_to_option_string(options.libdirs, LINKER_LIBDIR_FLAG)
  libs = array_to_option_string(options.libs, LINKER_LIB_FLAG)
  name = name .. EXESUFFIX
  if type(objects) == 'string' then objects = {objects} end
  local cmd = joinargs(LD, LDFLAGS, LINKER_OUTPUT_FLAG .. name,
    objects, libdirs, libs)
  text = '^ LD ' .. name .. '^ '
  tup.definerule{inputs=objects, command=text .. cmd, outputs={name}}
end

if DEBUG then
  table.insert(OPTIONS.define, 'RECKLESS_DEBUG')
end

tup.append_table(OPTIONS.libs, {'pthread'})
