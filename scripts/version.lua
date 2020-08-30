require('strict').on()

local function xci_read_software_version(dst_addr, msb_object_id, lsb_object_id)
	return xp.unpack_software_version(
		xp():read_user_info(dst_addr, msb_object_id),
		xp():read_user_info(dst_addr, lsb_object_id));
end

return {
	xtender = xci_read_software_version(101, 3130, 3131),
	variotrack = xci_read_software_version(301, 11050, 11051),
	bsp = xci_read_software_version(601, 7037, 7038),
}
