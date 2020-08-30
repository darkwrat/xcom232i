require('strict').on()

return {
	100 == xp.unpack_le_float(xp.pack_le_float(100)),
	100 == xp.unpack_le16(xp.pack_le16(100)),
	100 == xp.unpack_le32(xp.pack_le32(100)),
	true == xp.unpack_bool(xp.pack_bool(true)),
	false == xp.unpack_bool(xp.pack_bool(false)),
	'\x01\0\0\0' == xp.pack_signal(),
}
