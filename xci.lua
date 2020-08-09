#!/usr/bin/env tarantool

require('strict').on()

local xcic = require('xcic')

function xci_print_info(xp)
	print(string.format('Ubat = %.2f V (Battery voltage)', xcic.read_le_float(xp:read_user_info(101, 3000))))
	print(string.format('SOC = %.2f %% (State of charge)', xcic.read_le_float(xp:read_user_info(101, 3007))))
	print(string.format('Tbat = %.2f C (Battery temperature)', xcic.read_le_float(xp:read_user_info(101, 3001))))
	print(string.format('U in = %.2f V (Input voltage)', xcic.read_le_float(xp:read_user_info(101, 3011))))
	print(string.format('U out = %.2f V (Output voltage)', xcic.read_le_float(xp:read_user_info(101, 3021))))
	print(string.format('I in = %.2f A (Input current)', xcic.read_le_float(xp:read_user_info(101, 3012))))
	print(string.format('I out = %.2f A (Output current)', xcic.read_le_float(xp:read_user_info(101, 3022))))
	print(string.format('PVxP = %.2f kW (Max power production for the current day)', xcic.read_le_float(xp:read_user_info(301, 11019))))
	print(string.format('Psol = %.2f kW (Power of the PV generator)', xcic.read_le_float(xp:read_user_info(301, 11004))))
	print(string.format('Upv = %.2f V (Voltage of the PV generator)', xcic.read_le_float(xp:read_user_info(301, 11002))))
	print(string.format('Ed = %.2f kWh (Production in (kWh) for the current day)', xcic.read_le_float(xp:read_user_info(301, 11007))))
end

local xp = xcic.open_port('/dev/ttyS0')

xci_print_info(xp)

print('----')

print(string.format('VALUE = %.2f Vdc (Battery overvoltage level)',
	xcic.read_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_VALUE_QSP)
	)))
print(string.format('MIN = %.2f Vdc (Battery overvoltage level)',
	xcic.read_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_MIN_QSP)
	)))
print(string.format('MAX = %.2f Vdc (Battery overvoltage level)',
	xcic.read_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_MAX_QSP)
	)))
print(string.format('LEVEL = 0x%x (Battery overvoltage level)',
	xcic.read_le16(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_LEVEL_QSP)
	)))
--print(string.format('UNSAVED_VALUE = %.2f Vdc (Battery overvoltage level)',
--	xcic.read_le_float(
--		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_UNSAVED_VALUE_QSP)
--	)))
