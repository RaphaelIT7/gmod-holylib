local angleUp = Angle().Up
local nativeUp = FindMetaTable("Angle").Up
local epsilon = 0.00001
local components = { -360, -180, -90, -30, 0, 30, 90, 180, 360 }

local function expectVector(actual, expected, expect)
    expect( isvector(actual) ).to.beTrue()
    expect( math.abs(actual.x - expected.x) < epsilon ).to.beTrue()
    expect( math.abs(actual.y - expected.y) < epsilon ).to.beTrue()
    expect( math.abs(actual.z - expected.z) < epsilon ).to.beTrue()
end

return {
    groupName = "Angle:Up",
    cases = {
        {
            name = "Returns the expected up axis for cardinal rotations",
            func = function()
                expectVector(angleUp(Angle(0, 0, 0)), Vector(0, 0, 1), expect)
                expectVector(angleUp(Angle(0, 90, 0)), Vector(0, 0, 1), expect)
                expectVector(angleUp(Angle(90, 0, 0)), Vector(1, 0, 0), expect)
                expectVector(angleUp(Angle(0, 0, 90)), Vector(0, -1, 0), expect)
                expectVector(angleUp(Angle(0, 0, -90)), Vector(0, 1, 0), expect)
                expectVector(angleUp(Angle(0, 0, 180)), Vector(0, 0, -1), expect)
            end
        },
        {
            name = "Matches the native up axis across pitch, yaw and roll",
            func = function()
                for _, pitch in ipairs(components) do
                    for _, yaw in ipairs(components) do
                        for _, roll in ipairs(components) do
                            local angle = Angle(pitch, yaw, roll)
                            expectVector(angleUp(angle), nativeUp(angle), expect)
                        end
                    end
                end
            end
        },
        {
            name = "Returns a unit vector perpendicular to forward and right without changing the angle",
            func = function()
                for _, pitch in ipairs(components) do
                    for _, yaw in ipairs(components) do
                        for _, roll in ipairs(components) do
                            local angle = Angle(pitch, yaw, roll)
                            local up = angleUp(angle)
                            expect( math.abs(up:LengthSqr() - 1) < epsilon ).to.beTrue()
                            expect( math.abs(up:Dot(angle:Forward())) < epsilon ).to.beTrue()
                            expect( math.abs(up:Dot(angle:Right())) < epsilon ).to.beTrue()
                            expect( angle.p ).to.equal(pitch)
                            expect( angle.y ).to.equal(yaw)
                            expect( angle.r ).to.equal(roll)
                        end
                    end
                end
            end
        },
    }
}
