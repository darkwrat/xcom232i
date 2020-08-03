#include <tarantool/module.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <scom_property.h>

#define XCIC_LUA_UDATA_NAME "__tnt_xcic"

struct xcic_frame {
	char buffer[1024];
	scom_frame_t frame;
	scom_property_t property;
};

static int xcic_new_frame(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.new_frame(dst_addr)");

	struct xcic_frame *f =
	    (struct xcic_frame *)lua_newuserdata(L, sizeof(struct xcic_frame));

	memset(f, 0, sizeof(*f));

	scom_initialize_frame(&f->frame, f->buffer, sizeof(f->buffer));

	f->frame.src_addr = 1;
	f->frame.dst_addr = lua_tointeger(L, 1);

	luaL_getmetatable(L, XCIC_LUA_UDATA_NAME);
	lua_setmetatable(L, -2);

	return 1;
}

static int xcic_frame_encode_read_property(lua_State *L)
{
	if (lua_gettop(L) < 4)
		return luaL_error(
		    L, "Usage: frame:encode_read_propery(object_type, "
		       "object_id, property_id)");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	scom_initialize_property(&f->property, &f->frame);

	f->property.object_type = lua_tointeger(L, 2);
	f->property.object_id = lua_tointeger(L, 3);
	f->property.property_id = lua_tointeger(L, 4);

	scom_encode_read_property(&f->property);

	if (f->frame.last_error != SCOM_ERROR_NO_ERROR)
		return luaL_error(
		    L, "read property frame encoding failed with error %d",
		    (int)f->frame.last_error);

	return 0;
}

static int xcic_frame_encode(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: frame:pack_header()");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	scom_encode_request_frame(&f->frame);

	if (f->frame.last_error != SCOM_ERROR_NO_ERROR)
		return luaL_error(
		    L, "data link frame encoding failed with error %d",
		    (int)f->frame.last_error);

	lua_pushlstring(L, f->frame.buffer, scom_frame_length(&f->frame));

	return 1;
}

static int xcic_frame_flip(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: frame:flip()");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	scom_initialize_frame(&f->frame, f->frame.buffer, f->frame.buffer_size);

	memset(f->frame.buffer, 0, f->frame.buffer_size);

	return 0;
}

static int xcic_frame_decode_header(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Usage: frame:decode_header(data)");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	size_t data_len;
	const char *data = lua_tolstring(L, 2, &data_len);

	if (data_len != SCOM_FRAME_HEADER_SIZE)
		return luaL_error(
		    L, "error when reading the header from the com port");

	memcpy(f->frame.buffer, data, data_len);

	scom_decode_frame_header(&f->frame);

	if (f->frame.last_error != SCOM_ERROR_NO_ERROR)
		return luaL_error(
		    L, "data link header decoding failed with error %d",
		    (int)f->frame.last_error);

	return 0;
}

static int xcic_frame_data_len(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: frame:data_len()");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	lua_pushinteger(L,
			scom_frame_length(&f->frame) - SCOM_FRAME_HEADER_SIZE);

	return 1;
}

static int xcic_frame_decode_data(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Usage: frame:decode_data(data)");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	size_t data_len;
	const char *data = lua_tolstring(L, 2, &data_len);

	if (data_len != (scom_frame_length(&f->frame) - SCOM_FRAME_HEADER_SIZE))
		return luaL_error(
		    L, "error when reading the data from the com port");

	memcpy(&f->frame.buffer[SCOM_FRAME_HEADER_SIZE], data, data_len);

	scom_decode_frame_data(&f->frame);

	if (f->frame.last_error != SCOM_ERROR_NO_ERROR)
		return luaL_error(
		    L, "data link data decoding failed with error %d",
		    (int)f->frame.last_error);

	return 0;
}

static int xcic_frame_decode_read_property(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: frame:decode_read_property()");

	struct xcic_frame *f =
	    (struct xcic_frame *)luaL_checkudata(L, 1, XCIC_LUA_UDATA_NAME);

	scom_initialize_property(&f->property, &f->frame);

	scom_decode_read_property(&f->property);

	if (f->frame.last_error != SCOM_ERROR_NO_ERROR)
		return luaL_error(L,
				  "read property decoding failed with error %d",
				  (int)f->frame.last_error);

	lua_pushlstring(L, f->property.value_buffer, f->property.value_length);

	return 1;
}

static int xcic_setup_tty(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.setup_tty(fh)");

	int fh = lua_tointeger(L, 1);

	struct termios tty;
	if (tcgetattr(fh, &tty) != 0)
		return luaL_error(L, "tcgetattr: %s", strerror(errno));

	(void)cfsetospeed(&tty, B38400);
	(void)cfsetispeed(&tty, B38400);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK; // disable break processing
	tty.c_lflag = 0;	// no signaling chars, no echo,
				// no canonical processing
	tty.c_oflag = 0;	// no remapping, no delays
	tty.c_cc[VMIN] = -1;	// read blocks
	tty.c_cc[VTIME] = 20;	// 2.0 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls,
					 // enable reading
	tty.c_cflag |= PARENB;		 // enable parity
	tty.c_cflag &= ~PARODD;		 // even parity
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fh, TCSANOW, &tty) != 0)
		return luaL_error(L, "tcsetattr: %s", strerror(errno));

	return 0;
}

static int xcic_read_le32(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.read_le32(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		return luaL_error(L, "invalid le32 length");

	lua_pushinteger(L, scom_read_le32(data));

	return 1;
}

static int xcic_read_le16(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.read_le16(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 2)
		return luaL_error(L, "invalid le16 length");

	lua_pushinteger(L, scom_read_le16(data));

	return 1;
}

static int xcic_read_le_float(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.read_le_float(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		return luaL_error(L, "invalid le float length");

	lua_pushnumber(L, (lua_Number)scom_read_le_float(data));

	return 1;
}

/*
 * List of exporting: aliases, callbacks, definitions, functions etc [[
 */
struct define {
	const char *name;
	int value;
};

static const struct define defines[] = {
    {"FRAME_HEADER_SIZE", SCOM_FRAME_HEADER_SIZE},
    {"USER_INFO_OBJECT_TYPE", SCOM_USER_INFO_OBJECT_TYPE},
    {"PARAMETER_OBJECT_TYPE", SCOM_PARAMETER_OBJECT_TYPE},
    {NULL, 0}};

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
    {"setup_tty", xcic_setup_tty},	   {"new_frame", xcic_new_frame},
    {"read_le32", xcic_read_le32},	   {"read_le16", xcic_read_le16},
    {"read_le_float", xcic_read_le_float}, {NULL, NULL}};

static const struct luaL_Reg M[] = {
    {"encode_read_property", xcic_frame_encode_read_property},
    {"encode", xcic_frame_encode},
    {"flip", xcic_frame_flip},
    {"data_len", xcic_frame_data_len},
    {"decode_header", xcic_frame_decode_header},
    {"decode_data", xcic_frame_decode_data},
    {"decode_read_property", xcic_frame_decode_read_property},
    {NULL, NULL}};
/*
 * ]]
 */

/*
 * Lib initializer
 */
LUA_API int luaopen_xcic(lua_State *L)
{
	/**
	 * Add metatable.__index = metatable
	 */
	luaL_newmetatable(L, XCIC_LUA_UDATA_NAME);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, M);
	luaL_register(L, NULL, R);

	/**
	 * Add definitions
	 */
	const struct define *defs = &defines[0];
	while (defs->name) {
		lua_pushinteger(L, defs->value);
		lua_setfield(L, -2, defs->name);
		defs++;
	}

	return 1;
}
