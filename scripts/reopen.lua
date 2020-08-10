require('strict').on()

xp:close()
xp = xcic.open_port('/dev/ttyS0')
return xp
