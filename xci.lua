#!/usr/bin/env tarantool

require('strict').on()
require("console").listen(3302)

box.cfg{
    listen = '*:3301',
    force_recovery = false,
    wal_mode = 'none',
}

