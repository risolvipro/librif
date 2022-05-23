local test = 2

librif_test = {}

if test == 1 then
    import "tests/test1"
elseif test == 2 then
    import "tests/test2"
elseif test == 3 then
    import "tests/test3"
end
function playdate.update()
    librif_test.update()
end