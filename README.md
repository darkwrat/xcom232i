# xcom232i

Stuff for interfacing with Xcom-232i through RS-232 serial port.

- usermod -a -G dialout tarantool
- tarantoolctl rocks install http
- tarantoolctl rocks install metrics

Export xp and xcic globally in the instance file:

```
xcic = require('xcic')
xp = xcic.open_port('/dev/ttyS0')
```

Use scripts like this:

```
# tarantoolctl eval xci scripts/version.lua
connected to unix/:/var/run/tarantool/xci.control
---
- variotrack: 1.6.30
  bsp: 1.6.28
...

```

Use one-liners like this:

```
# echo 'xcic.unpack_le_float(xp:read_user_info(101, 3000))' |tarantoolctl eval xci
connected to unix/:/var/run/tarantool/xci.control
---
- 50.1875
...

```
