#!/usr/bin/env tarantool

require('strict').on()

local xcic = require('xcic')

local xp = xcic.open_port('/dev/ttyS0')
local data = xp:read_user_info(101, 3000)
print(string.format('uBat = %.2f V', xcic.read_le_float(data)))
