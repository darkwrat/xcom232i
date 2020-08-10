require('strict').on()

local metrics = require('metrics')

local http_router = require('http.router').new()
local http_handler = require('metrics.plugins.prometheus').collect_http
local http_server = require('http.server').new('0.0.0.0', 8088)

local function xci_metric_callback(self)
	-- xtender
	self.gauge.xt_battery_voltage:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3000)
		))
	self.gauge.xt_battery_soc:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3007)
		))
	self.gauge.xt_battery_temperature:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3001)
		))
	self.gauge.xt_input_voltage:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3011)
		))
	self.gauge.xt_output_voltage:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3021)
		))
	self.gauge.xt_input_current:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3012)
		))
	self.gauge.xt_output_current:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3022)
		))
	self.gauge.xt_input_frequency:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3014)
		))
	self.gauge.xt_output_frequency:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3024)
		))

	-- variotrack
	self.gauge.vt_ed:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11007)
		))
	self.gauge.vt_pvxp:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11019)
		))
	self.gauge.vt_psol:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11004)
		))
	self.gauge.vt_upv:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11002)
		))
end

local xci_metric = {
	counter = setmetatable({}, {
		__index = function(self, k)
			local cc = rawget(self, k)
			if cc == nil then
				cc = metrics.counter('xci_' .. k)
				self.k = cc
			end
			return cc
		end}),
	gauge = setmetatable({}, {
		__index = function(self, k)
			local gg = rawget(self, k)
			if gg == nil then
				gg = metrics.gauge('xci_' .. k)
				self.k = gg
			end
			return gg
		end}),
	histogram = setmetatable({}, {
		__index = function(self, k)
			local hh = rawget(self, k)
			if hh == nil then
				hh = metrics.histogram('xci_' .. k)
				self.k = hh
			end
			return hh
		end}),
}

return {
	start = function()
		metrics.register_callback(setmetatable(xci_metric, {__call = xci_metric_callback}))

		http_server:set_router(http_router)
		http_router:route({path = '/metrics'}, function(...) return http_handler(...) end)
		http_server:start()
	end
}
