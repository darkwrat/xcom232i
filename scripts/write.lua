require('strict').on()

local log = require('log')

local before = xp.unpack_le_float(
		xp():read_parameter_property(101, 1309, 0x5)
		)

xp():write_parameter_property(101, 1309, 0x5, xp.pack_le_float(175)) -- default 180

local after = xp.unpack_le_float(
		xp():read_parameter_property(101, 1309, 0x5)
		)

return {
	before = before,
	after = after,
}
