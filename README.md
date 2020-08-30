# xcom232i

![XCI Dashboard Preview](https://raw.githubusercontent.com/darkwrat/xcom232i/master/etc/dashboard.png)

Stuff for interfacing with Studer Innotec Xcom-232i through RS-232 serial port.

- usermod -a -G dialout tarantool
- tarantoolctl rocks install http
- tarantoolctl rocks install metrics

Create a symlink to xci_init.lua from /etc/tarantool/instances.available/xci.lua

Use scripts like this:

```
# tarantoolctl eval xci scripts/version.lua
connected to unix/:/var/run/tarantool/xci.control
---
- bsp: 1.6.28
  xtender: 1.6.30
  variotrack: 1.6.30
...
```

Use one-liners like this:

```
# echo 'xp.unpack_le_float(xp():read_user_info(101, 3000))' |tarantoolctl eval xci
connected to unix/:/var/run/tarantool/xci.control
---
- 50.1875
...

```
