require('strict').on()

fs = fsc.new_fs()
fs:mount('/tmp/xxx')
fs:run_loop()

return {}
