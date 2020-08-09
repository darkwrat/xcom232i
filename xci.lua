require('strict').on()

local xcic = require('xcic')
local metrics = require('metrics')

local http_router = require('http.router').new()
local http_handler = require('metrics.plugins.prometheus').collect_http
local http_server = require('http.server').new('0.0.0.0', 8088)

local xp = xcic.open_port('/dev/ttyS0')

local xci_xt_battery_voltage_gauge =
	metrics.gauge('xci_xt_battery_voltage')
local xci_xt_battery_soc_gauge =
	metrics.gauge('xci_xt_battery_soc')
local xci_xt_battery_temperature_gauge =
	metrics.gauge('xci_xt_battery_temperature')
local xci_xt_input_voltage_gauge =
	metrics.gauge('xci_xt_input_voltage')
local xci_xt_output_voltage_gauge =
	metrics.gauge('xci_xt_output_voltage')
local xci_xt_input_current_gauge =
	metrics.gauge('xci_xt_input_current')
local xci_xt_output_current_gauge =
	metrics.gauge('xci_xt_output_current')
local xci_xt_input_frequency_gauge =
	metrics.gauge('xci_xt_input_frequency')
local xci_xt_output_frequency_gauge =
	metrics.gauge('xci_xt_output_frequency')

local xci_vt_ed_gauge =
	metrics.gauge('xci_vt_ed')
local xci_vt_pvxp_gauge =
	metrics.gauge('xci_vt_pvxp')
local xci_vt_psol_gauge =
	metrics.gauge('xci_vt_psol')
local xci_vt_upv_gauge =
	metrics.gauge('xci_vt_upv')

local function xci_metrics_callback()
	-- xtender
	xci_xt_battery_voltage_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3000)
		))
	xci_xt_battery_soc_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3007)
		))
	xci_xt_battery_temperature_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3001)
		))
	xci_xt_input_voltage_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3011)
		))
	xci_xt_output_voltage_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3021)
		))
	xci_xt_input_current_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3012)
		))
	xci_xt_output_current_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3022)
		))
	xci_xt_input_frequency_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3084)
		))
	xci_xt_output_frequency_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(101, 3085)
		))

	-- variotrack
	xci_vt_ed_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11007)
		))
	xci_vt_pvxp_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11019)
		))
	xci_vt_psol_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11004)
		))
	xci_vt_upv_gauge:set(
		xcic.unpack_le_float(
			xp:read_user_info(301, 11002)
		))
end

local function xci_start()
	metrics.register_callback(xci_metrics_callback)

	http_server:set_router(http_router)
	http_router:route({path = '/metrics'}, function(...) return http_handler(...) end)
	http_server:start()
end


return {
	start = xci_start
}
