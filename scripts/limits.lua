require('strict').on()

return {
	transfer = {
		allowed = xp.unpack_bool(
			xp():read_parameter_property(101, 1128, 0xD)
			),
		delay = {
			voltage_open_sec = xp.unpack_le_float(
				xp():read_parameter_property(101, 1198, 0xD)
				),
			frequency_open_sec = xp.unpack_le_float(
				xp():read_parameter_property(101, 1507, 0xD)
				),
			close_sec = xp.unpack_le_float(
				xp():read_parameter_property(101, 1580, 0xD)
				),
		},
	},
	voltage = {
		under_vac = xp.unpack_le_float(
			xp():read_parameter_property(101, 1199, 0xD)
			),
		under_immediate_vac = xp.unpack_le_float(
			xp():read_parameter_property(101, 1200, 0xD)
			),
		absolute_max_vac = xp.unpack_le_float(
			xp():read_parameter_property(101, 1432, 0xD)
			),
	},
	frequency = {
		over_hz = xp.unpack_le_float(
			xp():read_parameter_property(101, 1505, 0xD)
			),
		under_hz = xp.unpack_le_float(
			xp():read_parameter_property(101, 1506, 0xD)
			),
	},
	current = {
		max_aac = xp.unpack_le_float(
			xp():read_parameter_property(101, 1107, 0xD)
			),
	},
}
