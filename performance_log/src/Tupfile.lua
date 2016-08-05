table.insert(OPTIONS.includes, joinpath(tup.getcwd(), '../include'))
for i, name in ipairs(filter_platform_files(tup.glob("*.cpp"))) do
    compile(name)
end
