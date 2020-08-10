#!/usr/bin/env tarantool

box.cfg{
	listen = 3301,
	custom_proc_title = 'xci',
	memtx_memory = 32 * 1024 * 1024,
	work_dir = '/xci',
	wal_dir = '/xci/xlogs',
	memtx_dir = '/xci/snaps',
	vinyl_dir = '/xci/vinyl',
	read_only = false,
}

package.path = package.path .. ';/usr/share/tarantool/.rocks/share/tarantool/?.lua'
package.cpath = package.cpath .. ';/usr/share/tarantool/?.so'

xcic = require('xcic')
xp = xcic.open_port('/dev/ttyS0')

require('xci').start()
