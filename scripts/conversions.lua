require('strict').on()

return {
	100 == xcic.unpack_le_float(xcic.pack_le_float(100)),
	100 == xcic.unpack_le16(xcic.pack_le16(100)),
	100 == xcic.unpack_le32(xcic.pack_le32(100)),
	true == xcic.unpack_bool(xcic.pack_bool(true)),
	false == xcic.unpack_bool(xcic.pack_bool(false)),
	"\x01" == xcic.pack_signal(),
}
