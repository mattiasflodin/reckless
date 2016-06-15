function string.startswith(s, suffix)
  return suffix=='' or string.sub(s, 1, string.len(suffix))==suffix
end

function string.endswith(s, suffix)
  return suffix=='' or string.sub(s, -string.len(suffix))==suffix
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

function join(...)
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

DEFINE = {}
INCLUDES = {}
LIBS = {}
LIBDIRS = {}

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
  
  CFLAGS = '-c -ggdb3 -std=c++11 -Wall -Werror'
  LDFLAGS = '-g'
  ARFLAGS = 'crs'
  
  COMPILER_OUTPUT_FLAG = '-o'
  COMPILER_INCLUDE_FLAG = '-I'
  LINKER_OUTPUT_FLAG = '-o'
  LINKER_LIBDIR_FLAG = '-L'
  LINKER_LIB_FLAG = '-l'
  
  OBJSUFFIX = '.o'
  EXESUFFIX = ''
  LIBPREFIX = 'lib'
  LIBSUFFIX = '.a'
  
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
  CFLAGS = '/c'
  CFLAGS = CFLAGS .. ' /EHsc' -- C++ exception handling
  CFLAGS = CFLAGS .. ' /Zc:forScope /Zc:wchar_t /Zc:auto /Zc:rvalueCast /Zc:strictStrings /Zc:implicitNoexcept /Zc:inline /Zc:throwingNew /Zc:referenceBinding' -- C++11 conformance
  CFLAGS = CFLAGS .. ' /fp:precise' -- Predictable floating-point operations
  CFLAGS = CFLAGS .. ' /W3 /WX' -- Warning level 3, warnings as errors
  CFLAGS = CFLAGS .. ' /GS /sdl' -- Security checks
  CFLAGS = CFLAGS .. ' /Zi' -- Generate debug PDB
  CFLAGS = CFLAGS .. ' /Gm-'  -- Disable minimal rebuild
  CFLAGS = CFLAGS .. ' /FS'  -- Force serialized access to PDB file
  CFLAGS = CFLAGS .. ' /nologo'
  CFLAGS = CFLAGS .. ' /Fd' .. join(tup.getcwd(), 'reckless.pdb')
  LDFLAGS = ''
  ARFLAGS = '/NOLOGO'

  COMPILER_OUTPUT_FLAG = '/Fo:'
  COMPILER_INCLUDE_FLAG = '/I'
  LINKER_OUTPUT_FLAG = '/OUT:'
  LINKER_LIBDIR_FLAG = '/LIBPATH:'
  LINKER_LIB_FLAG = ''
  
  EXESUFFIX = '.exe'
  LIBPREFIX = ''
  LIBSUFFIX = '.lib'
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

function compile(name)
  -- Include directories
  include_args = array_to_option_string(INCLUDES, COMPILER_INCLUDE_FLAG)
  
  -- Object file name
  local base = tup.base(name)
  local objname = base .. OBJSUFFIX

  local cmd = table.concat({CXX, CFLAGS, OPTIMIZATION, include_args,
    COMPILER_OUTPUT_FLAG .. objname, name}, ' ')
  local text = '^ CXX ' .. name .. '^ '
  tup.definerule{inputs={name}, command=text .. cmd, outputs={objname}}
  return objname
end

function library(name, objects)
  name = LIBPREFIX .. name .. LIBSUFFIX
  local cmd = {AR, ARFLAGS, name, table.concat(objects, ' ')}
  cmd = table.concat(cmd, ' ')
  
  text = '^ AR ' .. name .. '^ '
  tup.definerule{inputs=objects, command=text .. cmd, outputs={name}}
end

function link(name, objects)
  libdirs = array_to_option_string(LIBDIRS, LINKER_LIBDIR_FLAG)
  libs = array_to_option_string(LIBS, LINKER_LIB_FLAG)
  name = name .. EXESUFFIX
  if type(objects) == 'string' then objects = {objects} end
  local cmd = {LD, LDFLAGS, LINKER_OUTPUT_FLAG .. name,
    table.concat(objects, ' '), libdirs, libs}
  cmd = table.concat(cmd, ' ')
  text = '^ LD ' .. name .. '^ '
  tup.definerule{inputs=objects, command=text .. cmd, outputs={name}}
end

if DEBUG then
  table.insert(DEFINE, 'RECKLESS_DEBUG')
end

tup.append_table(LIBS, {'pthread'})
