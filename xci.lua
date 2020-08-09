#!/usr/bin/env tarantool

require('strict').on()

local xcic = require('xcic')

function xci_read_software_version(xp, dst_addr, msb_object_id, lsb_object_id)
	return xcic.unpack_software_version(
		xp:read_user_info(dst_addr, msb_object_id),
		xp:read_user_info(dst_addr, lsb_object_id));
end

function xci_print_info(xp)
	-- xtender
	print(string.format('Ubat = %.2f V (Battery voltage)',
		xcic.unpack_le_float(
			xp:read_user_info(101, 3000)
		)))
	print(string.format('SOC = %.2f %% (State of charge)',
		xcic.unpack_le_float(
			xp:read_user_info(101, 3007)
		)))
	print(string.format('Tbat = %.2f C (Battery temperature)',
		xcic.unpack_le_float(
			xp:read_user_info(101, 3001)
		)))
	print(string.format('U in = %.2f V (Input voltage)',
		xcic.unpack_le_float(xp:read_user_info(101, 3011)
		)))
	print(string.format('U out = %.2f V (Output voltage)',
		xcic.unpack_le_float(xp:read_user_info(101, 3021)
		)))
	print(string.format('I in = %.2f A (Input current)',
		xcic.unpack_le_float(
			xp:read_user_info(101, 3012)
		)))
	print(string.format('I out = %.2f A (Output current)',
		xcic.unpack_le_float(
			xp:read_user_info(101, 3022)
		)))

	-- variotrack
	print(string.format('PVxP = %.2f kW (Max power production for the current day)',
		xcic.unpack_le_float(
			xp:read_user_info(301, 11019)
		)))
	print(string.format('Psol = %.2f kW (Power of the PV generator)',
		xcic.unpack_le_float(
			xp:read_user_info(301, 11004)
		)))
	print(string.format('Upv = %.2f V (Voltage of the PV generator)',
		xcic.unpack_le_float(
			xp:read_user_info(301, 11002)
		)))
	print(string.format('Ed = %.2f kWh (Production in (kWh) for the current day)',
		xcic.unpack_le_float(
			xp:read_user_info(301, 11007)
		)))
end

local xp = xcic.open_port('/dev/ttyS0')

print('----')

print(string.format('VarioTrack software version = %s', xci_read_software_version(xp, 301, 11050, 11051)))
print(string.format('BSP software version = %s', xci_read_software_version(xp, 601, 7037, 7038)))

print('----')

xci_print_info(xp)

print('----')

print(string.format('VALUE = %.2f Vdc (Battery overvoltage level)',
	xcic.unpack_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_VALUE_QSP)
	)))
print(string.format('MIN = %.2f Vdc (Battery overvoltage level)',
	xcic.unpack_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_MIN_QSP)
	)))
print(string.format('MAX = %.2f Vdc (Battery overvoltage level)',
	xcic.unpack_le_float(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_MAX_QSP)
	)))
print(string.format('LEVEL = 0x%x (Battery overvoltage level)',
	xcic.unpack_le16(
		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_LEVEL_QSP)
	)))
--print(string.format('UNSAVED_VALUE = %.2f Vdc (Battery overvoltage level)',
--	xcic.unpack_le_float(
--		xp:read_parameter_property(101, 1121, xcic.PARAMETER_PROPERTY_UNSAVED_VALUE_QSP)
--	)))

print('----')
