require('strict').on()

local metrics = require('metrics')

local http_router = require('http.router').new()
local http_handler = require('metrics.plugins.prometheus').collect_http
local http_server = require('http.server').new('0.0.0.0', 8088)

local function xci_metric_callback(self)
	-- xtender
	self.gauge.xt_ubat_min:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3090)
		))
	self.gauge.xt_uin:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3113)
		))
	self.gauge.xt_iin:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3116)
		))
	self.gauge.xt_pout:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3098)
		))
	self.gauge.xt_pout_plus:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3097)
		))
	self.gauge.xt_fout:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3110)
		))
	self.gauge.xt_fin:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3122)
		))
	self.gauge.xt_phase:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3010)
		))
	self.gauge.xt_state:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3049)
		))
	self.gauge.xt_mode:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3028)
		))
	self.gauge.xt_transfert:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3020)
		))
	self.gauge.xt_rel_out:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3030)
		))
	self.gauge.xt_rel_gnd:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3074)
		))
	self.gauge.xt_rel_neutral:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3075)
		))
	self.gauge.xt_rme:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3086)
		))
	self.gauge.xt_aux1:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3031)
		))
	self.gauge.xt_aux2:set(
		xcic.unpack_le16(
			xp:read_user_info(101, 3032)
		))
	self.gauge.xt_ubat:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3092)
		))
	self.gauge.xt_ibat:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3095)
		))
	self.gauge.xt_pin_a:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3119)
		))
	self.gauge.xt_pout_a:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3101)
		))
	self.gauge.xt_dev1_plus:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3103)
		))

	-- variotrack
	self.gauge.vt_psom:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11043)
		))
	self.gauge.vt_state:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11069)
		))
	self.gauge.vt_mode:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11016)
		))
	self.gauge.vt_dev1:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11045)
		))
	self.gauge.vt_upvm:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11041)
		))
	self.gauge.vt_ibam:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11040)
		))
	self.gauge.vt_ubam:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11039)
		))
	self.gauge.vt_phas:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11038)
		))
	self.gauge.vt_rme:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11082)
		))
	self.gauge.vt_aux1:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11061)
		))
	self.gauge.vt_aux2:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11062)
		))
	self.gauge.vt_aux3:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11077)
		))
	self.gauge.vt_aux4:set(
		xcic.unpack_le16(
			xp:read_user_info(301, 11078)
		))

	-- bsp
	self.gauge.bsp_ubat:set(
		xcic.unpack_le_float(
			xp:read_user_info(601, 7030)
		))
	self.gauge.bsp_ibat:set(
		xcic.unpack_le_float(
			xp:read_user_info(601, 7031)
		))
	self.gauge.bsp_soc:set(
		xcic.unpack_le_float(
			xp:read_user_info(601, 7032)
		))
	self.gauge.bsp_tbat:set(
		xcic.unpack_le_float(
			xp:read_user_info(601, 7033)
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
