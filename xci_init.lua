#!/usr/bin/env tarantool

fiber = require('fiber')
log = require('log')

package.path = package.path .. ';/usr/share/tarantool/.rocks/share/tarantool/?.lua'
package.cpath = package.cpath .. ';/usr/share/tarantool/?.so'

box.cfg{
	listen = 3301,
	custom_proc_title = 'xci',
	feedback_enabled = false,
	memtx_memory = 32 * 1024 * 1024,
	work_dir = '/xci',
	wal_dir = '/xci/xlogs',
	memtx_dir = '/xci/snaps',
	vinyl_dir = '/xci/vinyl',
	read_only = false,
}

box.once('xci_schema', function()
	local sm = box.schema.create_space('xci_message', { if_not_exists = true, })
	sm:create_index('pk', { type = 'tree', parts = { 1, 'unsigned', 2, 'unsigned', }, if_not_exists = true, })
	sm:format({
		-- 1 - receipt timestamp
		{ name = 'ts', type = 'unsigned', },
		-- 2 - source address
		{ name = 'src_addr', type = 'unsigned', },
		-- 3 - message type
		{ name = 'type', type = 'unsigned', },
		-- 4 - reserved value
		{ name = 'value', type = 'unsigned', },
	})
end)

local xcic = require('xcic')
local xpmt = {
	__call = function(self)
		local port = rawget(self, 'port')
		if port == nil or not port:usable() then
			port = xcic.open_port('/dev/ttyS0')
			log.info('xp: reopen (%s)', port)
			self.port = port
		end
		return port
	end
}

fsc = require('fsc')

xp = setmetatable(xcic, xpmt)

require('xci').start()
