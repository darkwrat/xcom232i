#!/usr/bin/env tarantool

require('strict').on()

local xcic = require('xcic')

function xci_read_software_version(xp, dst_addr, msb_object_id, lsb_object_id)
	return xcic.unpack_software_version(
		xp:read_user_info(dst_addr, msb_object_id),
		xp:read_user_info(dst_addr, lsb_object_id));
end

local xp = xcic.open_port('/dev/ttyS0')

--print(string.format('Xtender software version = %s',
--	xci_read_software_version(xp, 101, 3130, 3131)
--	))

print(string.format('VarioTrack software version = %s',
	xci_read_software_version(xp, 301, 11050, 11051)
	))

print(string.format('BSP software version = %s',
	xci_read_software_version(xp, 601, 7037, 7038)
	))
