require('strict').on()

local max_int = 18446744073709551614ULL

local s = box.space.xci_message

local vs = {
	[0] = 'Warning (000): Battery low',
	[1] = 'Warning (001): Battery too high',
	[2] = 'Warning (002): Bulk charge too long',
	[3] = '(003): AC-In synchronization in progress',
	[4] = 'Warning (004): Input frequency AC-In wrong',
	[5] = 'Warning (005): Input frequency AC-In wrong',
	[6] = 'Warning (006): Input voltage AC-In too high',
	[7] = 'Warning (007): Input voltage AC-In too low',
	[8] = 'Halted (008): Inverter overload SC',
	[9] = 'Halted (009): Charger short circuit',
	[10] = '(010): System start-up in progress',
	[11] = 'Warning (011): AC-In Energy quota',
	[12] = '(012): Use of battery temperature sensor',
	[13] = '(013): Use of additional remote control',
	[14] = 'Halted (014): Over temperature EL',
	[15] = 'Halted (015): Inverter overload BL',
	[16] = 'Warning (016): Fan error detected',
	[17] = '(017): Programing mode',
	[18] = 'Warning (018): Excessive battery voltage ripple',
	[19] = 'Halted (019): Battery undervoltage',
	[20] = 'Halted (020): Battery overvoltage',
	[21] = '(021): Transfer not authorized, AC-Out current is higher than {1107}',
	[22] = 'Halted (022): Voltage presence on AC-Out',
	[23] = 'Halted (023): Phase not defined',
	[24] = 'Warning (024): Change the clock battery',
	[25] = 'Halted (025): Unknown Command board. Software upgrade needed',
	[26] = 'Halted (026): Unknown Power board. Software upgrade needed',
	[27] = 'Halted (027): Unknown extension board. Software upgrade needed',
	[28] = 'Halted (028): Voltage incompatibility Power - Command',
	[29] = 'Halted (029): Voltage incompatibility Ext. - Command',
	[30] = 'Halted (030): Power incompatibility Power - Command',
	[31] = 'Halted (031): Command board software incompatibility',
	[32] = 'Halted (032): Power board software incompatibility',
	[33] = 'Halted (033): Extension board software incompatibility',
	[34] = 'Halted (034): FID corruption, call factory',
	[35] = '(035): Memory structure modified',
	[36] = 'Halted (036): Parameter file lacking',
	[37] = 'Warning (037): Message file lack. SW upgrade advised',
	[38] = 'Warning (038): Upgrade of the device software advised',
	[39] = 'Warning (039): Upgrade of the device software advised',
	[40] = 'Warning (040): Upgrade of the device software advised',
	[41] = 'Warning (041): Over temperature TR',
	[42] = 'Halted (042): Unauthorized energy source at the output',
	[43] = '(043): Start of monthly test',
	[44] = '(044): End of successfully monthly test',
	[45] = 'Warning (045): Monthly autonomy test failed',
	[46] = '(046): Start of weekly test',
	[47] = '(047): End of successfully weekly test',
	[48] = 'Warning (048): Weekly autonomy test failed',
	[49] = '(049): Transfer opened because AC-In max current exceeded {1107}',
	[50] = 'Error (050): Incomplete data transfer',
	[51] = '(051): The update is finished',
	[52] = '(052): Your installation is already updated',
	[53] = 'Halted (053): Devices not compatible, software update required',
	[54] = '(054): Please wait. Data transfer in progress',
	[55] = 'Error (055): No SD card inserted',
	[56] = 'Warning (056): Upgrade of the RCC software advised',
	[57] = '(057): Operation finished successfully',
	[58] = 'Halted (058): Master synchronization missing',
	[59] = 'Halted (059): Inverter overload HW',
	[60] = 'Warning (060): Time security 1512 AUX1',
	[61] = 'Warning (061): Time security 1513 AUX2',
	[62] = 'Warning (062): Genset, no AC-In coming after AUX command',
	[63] = '(063): Save parameter XT',
	[64] = '(064): Save parameter BSP',
	[65] = '(065): Save parameter VarioTrack',
	[71] = 'Error (071): Insufficient disk space on SD card',
	[72] = 'Halted (072): COM identification incorrect',
	[73] = '(073): Datalogger is enabled on this RCC',
	[74] = '(074): Save parameter Xcom-MS',
	[75] = '(075): MPPT MS address changed successfully',
	[76] = 'Error (076): Error during change of MPPT MS address',
	[77] = 'Error (077): Wrong MPPT MS DIP Switch position',
	[78] = '(078): SMS or email sent',
	[79] = 'Halted (079): More than 9 XTs in the system',
	[80] = 'Halted (080): No battery (or reverse polarity)',
	[81] = 'Warning (081): Earthing fault',
	[82] = 'Halted (082): PV overvoltage',
	[83] = 'Warning (083): No solar production in the last 48h',
	[84] = '(084): Equalization performed',
	[85] = 'Error (085): Modem not available',
	[86] = 'Error (086): Incorrect PIN code, unable to initiate the modem',
	[87] = 'Error (087): Insufficient Signal from GSM modem',
	[88] = 'Error (088): No connection to GSM network',
	[89] = 'Error (089): No Xcom server access',
	[90] = '(090): Xcom server connected',
	[91] = 'Warning (091): Update finished. Update software of other RCC/Xcom-232i',
	[92] = 'Error (092): More than 4 RCC or Xcom in the system',
	[93] = 'Error (093): More than 1 BSP in the system',
	[94] = 'Error (094): More than 1 Xcom-MS in the system',
	[95] = 'Error (095): More than 15 VarioTrack in the system',
	[121] = 'Error (121): Impossible communication with target device',
	[122] = 'Error (122): SD card corrupted',
	[123] = 'Error (123): SD card not formatted',
	[124] = 'Error (124): SD card not compatible',
	[125] = 'Error (125): SD card format not recognized. Should be FAT',
	[126] = 'Error (126): SD card write protected',
	[127] = 'Error (127): SD card, file(s) corrupted',
	[128] = 'Error (128): SD card file or directory could not be found',
	[129] = 'Error (129): SD card has been prematurely removed',
	[130] = 'Error (130): Update directory is empty',
	[131] = '(131): The VarioTrack is configured for 12V batteries',
	[132] = '(132): The VarioTrack is configured for 24V batteries',
	[133] = '(133): The VarioTrack is configured for 48V batteries',
	[134] = '(134): Reception level of the GSM signal',
	[137] = '(137): VarioTrack master synchronization lost',
	[138] = 'Error (138): XT master synchronization lost',
	[139] = '(139): Synchronized on VarioTrack master',
	[140] = '(140): Synchronized on XT master',
	[141] = 'Error (141): More than 1 Xcom-SMS in the system',
	[142] = 'Error (142): More than 15 VarioString in the system',
	[143] = '(143): Save parameter Xcom-SMS',
	[144] = '(144): Save parameter VarioString',
	[145] = 'Error (145): SIM card blocked, PUK code required',
	[146] = 'Error (146): SIM card missing',
	[147] = 'Error (147): Install R532 firmware release prior to install an older release',
	[148] = '(148): Datalogger function interrupted (SD card removed)',
	[149] = 'Error (149): Parameter setting incomplete',
	[150] = 'Error (150): Cabling error between PV and VarioString',
	[162] = 'Error (162): Communication loss with RCC or Xcom-232i',
	[163] = 'Error (163): Communication loss with Xtender',
	[164] = 'Error (164): Communication loss with BSP',
	[165] = 'Error (165): Communication loss with Xcom-MS',
	[166] = 'Error (166): Communication loss with VarioTrack',
	[167] = 'Error (167): Communication loss with VarioString',
	[168] = '(168): Synchronized with VarioString master',
	[169] = '(169): Synchronization with VarioString master lost',
	[170] = 'Warning (170): No solar production in the last 48h on PV1',
	[171] = 'Warning (171): No solar production in the last 48h on PV2',
	[172] = 'Error (172): FID change impossible. More than one unit.',
	[173] = 'Error (173): Incompatible Xtender. Please contact Studer Innotec SA',
	[174] = '(174): Inaccessible parameter, managed by the Xcom-CAN',
	[175] = 'Halted (175): Critical undervoltage',
	[176] = '(176): Calibration setting lost',
	[177] = '(177): An Xtender has started up',
	[178] = '(178): No BSP. Necessary for programming with SOC',
	[179] = '(179): No BTS or BSP. Necessary for programming with temperature',
	[180] = '(180): Command entry activated',
	[181] = 'Error (181): Disconnection of BTS',
	[182] = '(182): BTS/BSP battery temperature measurement used by a device',
	[183] = 'Halted (183): An Xtender has lost communication with the system',
	[184] = 'Error (184): Check phase orientation or circuit breakers state on AC-In',
	[185] = 'Warning (185): AC-In voltage level with delay too low',
	[186] = 'Halted (186): Critical undervoltage (fast)',
	[187] = 'Halted (187): Critical overvoltage (fast)',
	[188] = '(188): CAN stage startup',
	[189] = 'Error (189): Incompatible configuration file',
	[190] = '(190): The Xcom-SMS is busy',
	[191] = '(191): Parameter not supported',
	[192] = '(192): Unknown reference',
	[193] = '(193): Invalid value',
	[194] = '(194): Value too low',
	[195] = '(195): Value too high',
	[196] = '(196): Writing error',
	[197] = '(197): Reading error',
	[198] = '(198): User level insufficient',
	[199] = '(199): No data for the report',
	[200] = 'Error (200): Memory full',
	[202] = 'Warning (202): Battery alarm arrives',
	[203] = '(203): Battery alarm leaves',
	[204] = 'Error (204): Battery stop arrives',
	[205] = '(205): Battery stop leaves',
	[206] = 'Halted (206): Board hardware incompatibility',
	[207] = '(207): AUX1 relay activation',
	[208] = '(208): AUX1 relay deactivation',
	[209] = '(209): AUX2 relay activation',
	[210] = '(210): AUX2 relay deactivation',
	[211] = '(211): Command entry deactivated',
	[212] = 'Error (212): VarioTrack software incompatibility. Upgrade needed',
	[213] = '(213): Battery current limitation by the BSP stopped',
	[214] = 'Warning (214): Half period RMS voltage limit exceeded, transfer opened',
	[215] = 'Warning (215): UPS limit reached, transfer opened',
	[216] = 'Warning (216): Scom watchdog caused the reset of Xcom-232i',
	[217] = 'Warning (217): CAN problem at Xtender declaration',
	[218] = 'Warning (218): CAN problem while writing parameters',
	[222] = '(222): Front ON/OFF button pressed',
	[223] = '(223): Main OFF detected',
	[224] = '(224): Delay before closing transfer relay in progress {1580}',
	[225] = 'Error (225): Communication with lithium battery lost',
	[226] = '(226): Communication with lithium battery restored',
	[227] = 'Error (227): Overload on high voltage DC side',
	[228] = 'Error (228): Startup error',
	[229] = 'Error (229): Short-circuit on high voltage DC side'
}


local n, t, a, ts, v = xp:read_message(501, 0)
s:put {ts, a, t, v}

for i=1,n-1 do
	n, t, a, ts, v = xp:read_message(501, i)
	s:put {ts, a, t, v}
end

local f = {}

for _, m in s.index.pk:pairs({max_int}, {iterator = box.index.LE}) do
	table.insert(f, {
		ts = os.date('%Y-%m-%d %H:%M:%S', m.ts),
		from = m.src_addr,
		v = vs[m.type],
	})
end

return f