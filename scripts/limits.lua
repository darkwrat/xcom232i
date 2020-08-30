require('strict').on()

return {
	frequency = {
		over_hz = xp.unpack_le_float(
			xp():read_parameter_property(101, 1505, 0xD)
			),
		under_hz = xp.unpack_le_float(
			xp():read_parameter_property(101, 1506, 0xD)
			),
		delay_before_transfer_sec = xp.unpack_le_float(
			xp():read_parameter_property(101, 1507, 0xD)
			),
	}
}
