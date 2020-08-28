require('strict').on()

return {
	frequency = {
		over_hz = xcic.unpack_le_float(
			xp:read_parameter_property(101, 1505, 0x5)
			),
		under_hz = xcic.unpack_le_float(
			xp:read_parameter_property(101, 1506, 0x5)
			),
		delay_before_transfer_sec = xcic.unpack_le_float(
			xp:read_parameter_property(101, 1507, 0x5)
			),
	}
}
