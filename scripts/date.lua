require('strict').on()

local ts = xp.unpack_le32(
	xp():read_parameter_property(501, 5002, 0xD)
	)

return { ts, os.date('!%Y-%m-%dT%TZ', ts), }
