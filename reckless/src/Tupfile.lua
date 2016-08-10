if tup.getconfig('TRACE_LOG') != '' then
  table.insert(OPTIONS.define, 'RECKLESS_ENABLE_TRACE_LOG')
end
for i, name in ipairs(filter_platform_files(tup.glob("*.cpp"))) do
  compile(name)
end
