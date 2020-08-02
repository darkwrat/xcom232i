#!/usr/bin/env tarantool

require('strict').on()

local xcic = require('xcic')
local fio = require('fio')

dst_addr = 101
object_id = 3000
property_id = 1

tty, err = fio.open('/dev/ttyS0', {'O_RDWR', 'O_NOCTTY', 'O_SYNC'})
xcic.setup_tty(tty.fh)

f = xcic.new_frame(dst_addr)
f:encode_read_property(xcic.USER_INFO_OBJECT_TYPE, object_id, property_id)
req = f:pack_header()
if not tty:write(req) then
	error('bad write')
end

f:swap()
data = tty:read(xcic.FRAME_HEADER_SIZE)
f:unpack_header(data)

data = tty:read(f:data_len())
f:unpack_data(data)
data = f:decode_read_property()

print(string.format('uBat = %.2f V', xcic.read_le_float(data)))
