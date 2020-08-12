require('strict').on()

--local frame = '\170\118\101\0\0\0\1\0\0\0\14\0\233\27\2\1\1\0\26\12\0\0\1\0\0\128\35\64\10\163'
local frame = '\170\118\101\0\0\0\1\0\0\0\12\0\231\23\2\1\1\0\56\43\0\0\1\0\1\0\104\20'

local frame_header = string.sub(frame, 1, xcic.FRAME_HEADER_SIZE-2)
local frame_header_without_start_byte = string.sub(frame, 2, xcic.FRAME_HEADER_SIZE-2)
local frame_header_checksum_bytes = string.sub(frame, xcic.FRAME_HEADER_SIZE-1, xcic.FRAME_HEADER_SIZE)

local frame_data = string.sub(frame, xcic.FRAME_HEADER_SIZE+1, -3)
local frame_data_checksum_bytes = string.sub(frame, -2)

return {
	frame,
	frame:len(),

	frame_header,
	frame_header:len(),

	frame_header_checksum_bytes,

	xcic.unpack_le16(frame_header_checksum_bytes),
	xcic.calc_checksum(frame_header_without_start_byte),

	frame_data,
	frame_data:len(),

	frame_data_checksum_bytes,

	xcic.unpack_le16(frame_data_checksum_bytes),
	xcic.calc_checksum(frame_data),
}
