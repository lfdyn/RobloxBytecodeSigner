local tables = [[C:/Users/user/source/repos/Alpen/artifacts/tests]]
local cases = {
    { "arithmetic", "return 6 * 7", 42 },
    { "nested", "local function add(a) return function(b) return a + b end end return add(19)(23)", 42 },
    { "strings", "return string.sub('bytecode-signature', 1, 8)", "bytecode" },
}

for _, case in cases do
    local ok, result = pcall(function()
        local closure, valid, invalid, status = testsigner(tables, case[2])
        assert(valid == 0 and invalid == 7 and status == 0, "native verification result mismatch")
        assert(type(closure) == "function", "LvmLoad did not return a function")
        assert(closure() == case[3], "native closure returned the wrong result")
        return "PASS"
    end)
    print("SignerValidation", case[1], ok, result)
    assert(ok, result)
end
print("SignerValidation ALL PASSED")
