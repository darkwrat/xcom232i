/*
Copyright (c) 2020 Maxim Galaganov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <tarantool/module.h>

#include <small/small.h>
#include <small/static.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <scom_property.h>

#define XCIC_PORT_LUA_UDATA_NAME "__tnt_xcic_port"

LUA_API int luaopen_xcic(lua_State *L);

static int xcic_open_port(lua_State *L);
static int xcic_calc_checksum(lua_State *L);

static int xcic_unpack_le32(lua_State *L);
static int xcic_unpack_le16(lua_State *L);
static int xcic_unpack_le_float(lua_State *L);
static int xcic_unpack_software_version(lua_State *L);
static int xcic_unpack_bool(lua_State *L);

static int xcic_port_close(lua_State *L);
static int xcic_port_usable(lua_State *L);
static int xcic_port_to_string(lua_State *L);
static int xcic_port_gc(lua_State *L);
static int xcic_port_read_user_info(lua_State *L);
static int xcic_port_read_parameter_property(lua_State *L);

/** Xcom-232i serial port handle. */
struct xcic_port {
	/** The file descriptor of the opened serial port. */
	int fd;
	/** Latch for mutual exclusion of DTE exchanges. */
	box_latch_t *latch;
};

static int xcic_intl_read_property(lua_State *L, struct xcic_port *xp,
				   scom_frame_t *f, scom_property_t *p);

static char *xcic_intl_scom_strerror(scom_error_t error);
static void xcic_intl_port_close(struct xcic_port *xp);

static int xcic_intl_port_exchange(lua_State *L, struct xcic_port *xp,
				   scom_frame_t *f);
static ssize_t xcic_intl_port_read(struct xcic_port *xp, void *buf,
				   size_t count);
static ssize_t xcic_intl_port_write(struct xcic_port *xp, void *buf,
				    size_t count);
static void xcic_intl_dump_faulty_response_frame(scom_frame_t *f);
static uint16_t xcic_intl_calc_checksum(const char *data, uint_fast16_t length);

static ssize_t xcic_intl_open_cb(va_list ap);

#define xcic_lua_except_to(label, L, ...)                                      \
	({                                                                     \
		(void)lua_pushfstring(L, __VA_ARGS__);                         \
		goto label;                                                    \
	})

#define xcic_lua_except(L, ...) xcic_lua_except_to(except, L, __VA_ARGS__)

int xcic_open_port(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.open_port(pathname)");

	struct xcic_port *xp =
	    (struct xcic_port *)lua_newuserdata(L, sizeof(*xp));

	memset(xp, 0, sizeof(*xp));

	const char *pathname = lua_tostring(L, 1);

	xp->fd = coio_call(xcic_intl_open_cb, pathname,
			   O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (xp->fd == -1)
		xcic_lua_except(L, "open: %s", strerror(errno));

	struct termios tty = {.c_cflag = CS8 | CLOCAL | CREAD | PARENB};

	(void)cfsetospeed(&tty, B38400);
	(void)cfsetispeed(&tty, B38400);

	if (tcsetattr(xp->fd, TCSANOW, &tty) == -1)
		xcic_lua_except(L, "tcsetattr: %s", strerror(errno));

	if (tcsetattr(xp->fd, TCSANOW, &tty) == -1)
		xcic_lua_except(L, "tcsetattr: %s", strerror(errno));

	xp->latch = box_latch_new();

	luaL_getmetatable(L, XCIC_PORT_LUA_UDATA_NAME);
	lua_setmetatable(L, -2);

	return 1;

except:
	xcic_intl_port_close(xp);

	return lua_error(L);
}

int xcic_port_usable(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:usable()");

	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	lua_pushboolean(L, xp->fd != -1);

	return 1;
}

int xcic_port_close(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:close()");

	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	xcic_intl_port_close(xp);

	return 0;
}

void xcic_intl_port_close(struct xcic_port *xp)
{
	int fd = xp->fd;
	xp->fd = -1;

	(void)coio_close(fd);
}

int xcic_port_to_string(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:tostring()");

	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	lua_pushfstring(L, "xcic_port: %p (%d)", xp, xp->fd);

	return 1;
}

int xcic_port_gc(lua_State *L)
{
	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	xcic_intl_port_close(xp);

	box_latch_delete(xp->latch);

	return 0;
}

int xcic_port_read_user_info(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(
		    L, "Usage: xp:read_user_info(dst_addr, object_id)");

	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t f;
	scom_property_t p;
	char buffer[1024]; // fixme

	scom_initialize_frame(&f, buffer, sizeof(buffer));

	f.src_addr = 1;
	f.dst_addr = lua_tointeger(L, 2);

	scom_initialize_property(&p, &f);

	p.object_type = SCOM_USER_INFO_OBJECT_TYPE;
	p.object_id = lua_tointeger(L, 3);
	p.property_id = 1;

	if (xcic_intl_read_property(L, xp, &f, &p))
		goto except;

	lua_pushlstring(L, p.value_buffer, p.value_length);

	return 1;

except:
	return lua_error(L);
}

int xcic_port_read_parameter_property(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L,
				  "Usage: xp:read_parameter_property(dst_addr, "
				  "object_id, property_id)");

	struct xcic_port *xp =
	    (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t f;
	scom_property_t p;
	char buffer[1024]; // fixme

	scom_initialize_frame(&f, buffer, sizeof(buffer));

	f.src_addr = 1;
	f.dst_addr = lua_tointeger(L, 2);

	scom_initialize_property(&p, &f);

	p.object_type = SCOM_PARAMETER_OBJECT_TYPE;
	p.object_id = lua_tointeger(L, 3);
	p.property_id = lua_tointeger(L, 4);

	if (xcic_intl_read_property(L, xp, &f, &p))
		goto except;

	lua_pushlstring(L, p.value_buffer, p.value_length);

	return 1;

except:
	return lua_error(L);
}

int xcic_intl_read_property(lua_State *L, struct xcic_port *xp, scom_frame_t *f,
			    scom_property_t *p)
{
	scom_encode_read_property(p);
	if (f->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(
		    L, "read property frame encoding failed with error %d (%s)",
		    f->last_error, xcic_intl_scom_strerror(f->last_error));

	scom_encode_request_frame(f);
	if (f->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(
		    L, "data link frame encoding failed with error %d (%s)",
		    f->last_error, xcic_intl_scom_strerror(f->last_error));

	if (xcic_intl_port_exchange(L, xp, f))
		goto except;

	scom_decode_frame_data(f);
	if (f->last_error != SCOM_ERROR_NO_ERROR) {
		xcic_intl_dump_faulty_response_frame(f);
		xcic_lua_except(
		    L, "data link data decoding failed with error %d (%s)",
		    f->last_error, xcic_intl_scom_strerror(f->last_error));
	}

	scom_initialize_property(p, f);

	scom_decode_read_property(p);
	if (f->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(
		    L, "read property decoding failed with error %d (%s)",
		    f->last_error, xcic_intl_scom_strerror(f->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error
}

int xcic_intl_port_exchange(lua_State *L, struct xcic_port *xp, scom_frame_t *f)
{
	ssize_t nb;

	box_latch_lock(xp->latch);

	nb = xcic_intl_port_write(xp, f->buffer, scom_frame_length(f));
	if (nb != (ssize_t)scom_frame_length(f))
		xcic_lua_except(L, "error when writing to the com port");

	scom_initialize_frame(f, f->buffer, f->buffer_size);
	memset(f->buffer, 0, f->buffer_size);

	nb = xcic_intl_port_read(xp, f->buffer, SCOM_FRAME_HEADER_SIZE);
	if (nb != SCOM_FRAME_HEADER_SIZE)
		xcic_lua_except(
		    L, "error when reading the header from the com port");

	scom_decode_frame_header(f);
	if (f->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(
		    L, "data link header decoding failed with error %d (%s)",
		    f->last_error, xcic_intl_scom_strerror(f->last_error));

	nb = xcic_intl_port_read(xp, &f->buffer[SCOM_FRAME_HEADER_SIZE],
				 scom_frame_length(f) - SCOM_FRAME_HEADER_SIZE);
	if (nb != (ssize_t)(scom_frame_length(f) - SCOM_FRAME_HEADER_SIZE))
		xcic_lua_except(
		    L, "error when reading the data from the com port");

	box_latch_unlock(xp->latch);

	return 0;

except:
	box_latch_unlock(xp->latch);

	return -1; // caller must invoke `lua_error`
}

ssize_t xcic_intl_port_read(struct xcic_port *xp, void *buf, size_t count)
{
	size_t l = count;
	ssize_t n = 0;
	void *p = buf;

	while (l > 0) {
		if (xp->fd == -1)
			break;

		int w = coio_wait(xp->fd, COIO_READ, 100);
		if (xp->fd == -1 || fiber_is_cancelled())
			break;
		else if (!(w & COIO_READ))
			continue;

		n = read(xp->fd, p, l);

		if (n == 0)
			break;
		else if (n == -1 && errno == EAGAIN)
			continue;
		else if (n == -1)
			return n;

		l -= n;
		p += n;
	}

	return count - l;
}

ssize_t xcic_intl_port_write(struct xcic_port *xp, void *buf, size_t count)
{
	size_t l = count;
	ssize_t n = 0;
	void *p = buf;

	while (l > 0) {
		if (xp->fd == -1)
			break;

		int w = coio_wait(xp->fd, COIO_WRITE, 100);
		if (xp->fd == -1 || fiber_is_cancelled())
			break;
		else if (!(w & COIO_WRITE))
			continue;

		n = write(xp->fd, p, l);

		if (n == 0)
			break;
		else if (n == -1 && errno == EAGAIN)
			continue;
		else if (n == -1)
			return n;

		l -= n;
		p += n;
	}

	return count - l;
}

void xcic_intl_dump_faulty_response_frame(scom_frame_t *f)
{
	if (!f->service_flags.is_response)
		return;

	char *hex = (char *)static_reserve(scom_frame_length(f) * 4 + 1);
	if (hex) {
		char *ptr = hex;
		for (size_t i = 0; i < scom_frame_length(f); i++)
			ptr += sprintf(ptr, "\\%d", (uint8_t)f->buffer[i]);

		*ptr = '\0';
	}

	uint16_t cs =
	    scom_read_le16(&f->buffer[SCOM_FRAME_HEADER_SIZE + f->data_length]);
	uint16_t real_cs = xcic_intl_calc_checksum(
	    &f->buffer[SCOM_FRAME_HEADER_SIZE], f->data_length);

	say_info("xcic: faulty response frame "
		 "error %d src %d dst %d len %d dlen %d cs %d real %d hex %s",
		 f->service_flags.error, f->src_addr, f->dst_addr,
		 scom_frame_length(f), f->data_length, cs, real_cs, hex ?: "-");
}

uint16_t xcic_intl_calc_checksum(const char *data, uint_fast16_t length)
{
	uint_fast8_t A = 0xFF, B = 0;

	while (length--) {
		A = (A + *data++) & 0xFF;
		B = (B + A) & 0xFF;
	}

	return (B & 0xFF) << 8 | (A & 0xFF);
}

char *xcic_intl_scom_strerror(scom_error_t error)
{
	switch (error) {
	case SCOM_ERROR_NO_ERROR:
		return "no_error";
	case SCOM_ERROR_INVALID_FRAME:
		return "invalid_frame";
	case SCOM_ERROR_DEVICE_NOT_FOUND:
		return "device_not_found";
	case SCOM_ERROR_RESPONSE_TIMEOUT:
		return "response_timeout";
	case SCOM_ERROR_SERVICE_NOT_SUPPORTED:
		return "service_not_supported";
	case SCOM_ERROR_INVALID_SERVICE_ARGUMENT:
		return "invalid_service_argument";
	case SCOM_ERROR_GATEWAY_BUSY:
		return "gateway_busy";
	case SCOM_ERROR_TYPE_NOT_SUPPORTED:
		return "type_not_supported";
	case SCOM_ERROR_OBJECT_ID_NOT_FOUND:
		return "object_id_not_found";
	case SCOM_ERROR_PROPERTY_NOT_SUPPORTED:
		return "property_not_supported";
	case SCOM_ERROR_INVALID_DATA_LENGTH:
		return "invalid_data_length";
	case SCOM_ERROR_PROPERTY_IS_READ_ONLY:
		return "property_is_read_only";
	case SCOM_ERROR_INVALID_DATA:
		return "invalid_data";
	case SCOM_ERROR_DATA_TOO_SMALL:
		return "data_too_small";
	case SCOM_ERROR_DATA_TOO_BIG:
		return "data_too_big";
	case SCOM_ERROR_WRITE_PROPERTY_FAILED:
		return "write_property_failed";
	case SCOM_ERROR_READ_PROPERTY_FAILED:
		return "read_property_failed";
	case SCOM_ERROR_ACCESS_DENIED:
		return "access_denied";
	case SCOM_ERROR_OBJECT_NOT_SUPPORTED:
		return "object_not_supported";
	case SCOM_ERROR_MULTICAST_READ_NOT_SUPPORTED:
		return "multicast_read_not_supported";
	case SCOM_ERROR_INVALID_SHELL_ARG:
		return "invalid_shell_arg";
	case SCOM_ERROR_STACK_PORT_NOT_FOUND:
		return "stack_port_not_found";
	case SCOM_ERROR_STACK_PORT_INIT_FAILED:
		return "stack_port_init_failed";
	case SCOM_ERROR_STACK_PORT_WRITE_FAILED:
		return "stack_port_write_failed";
	case SCOM_ERROR_STACK_PORT_READ_FAILED:
		return "stack_port_read_failed";
	case SCOM_ERROR_STACK_BUFFER_TOO_SMALL:
		return "stack_buffer_too_small";
	case SCOM_ERROR_STACK_PROPERTY_HEADER_DOESNT_MATCH:
		return "stack_property_header_doesnt_match";
	default:
		return "unknown";
	}
}

int xcic_calc_checksum(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.calc_checksum(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	lua_pushinteger(L, xcic_intl_calc_checksum(data, (uint16_t)data_len));

	return 1;
}

int xcic_unpack_le32(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le32(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		xcic_lua_except(L, "invalid le32 length");

	lua_pushinteger(L, scom_read_le32(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_unpack_le16(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le16(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 2)
		xcic_lua_except(L, "invalid le16 length");

	lua_pushinteger(L, scom_read_le16(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_unpack_le_float(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le_float(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		xcic_lua_except(L, "invalid le float length");

	lua_pushnumber(L, (lua_Number)scom_read_le_float(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_unpack_software_version(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(
		    L,
		    "Usage: xcic.unpack_software_version(msb_data, lsb_data)");

	size_t msb_data_len;
	const char *msb_data = lua_tolstring(L, 1, &msb_data_len);

	if (msb_data_len != 4)
		xcic_lua_except(L, "invalid msb data length");

	size_t lsb_data_len;
	const char *lsb_data = lua_tolstring(L, 2, &lsb_data_len);

	if (lsb_data_len != 4)
		xcic_lua_except(L, "invalid lsb data length");

	uint16_t msb = (uint16_t)scom_read_le_float(msb_data);
	uint16_t lsb = (uint16_t)scom_read_le_float(lsb_data);

	lua_pushfstring(L, "%d.%d.%d", (int)msb >> 8, (int)lsb >> 8,
			(int)lsb & 0xFF);

	return 1;

except:
	return lua_error(L);
}

int xcic_unpack_bool(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_bool(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 1)
		xcic_lua_except(L, "invalid bool length");

	lua_pushboolean(L, *data);

	return 1;

except:
	return lua_error(L);
}

ssize_t xcic_intl_open_cb(va_list ap)
{
	char *pathname = va_arg(ap, char *);
	int flags = va_arg(ap, int);

	return open(pathname, flags);
}

/*
 * List of exporting: aliases, callbacks, definitions, functions etc [[
 */
struct define {
	const char *name;
	int value;
};

static const struct define defines[] = {
    {"FRAME_HEADER_SIZE", SCOM_FRAME_HEADER_SIZE}, {NULL, 0}};

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
    {"open_port", xcic_open_port},
    {"calc_checksum", xcic_calc_checksum},
    {"unpack_le32", xcic_unpack_le32},
    {"unpack_le16", xcic_unpack_le16},
    {"unpack_le_float", xcic_unpack_le_float},
    {"unpack_le_float", xcic_unpack_le_float},
    {"unpack_software_version", xcic_unpack_software_version},
    {"unpack_bool", xcic_unpack_bool},
    {NULL, NULL}};

static const struct luaL_Reg M[] = {
    {"read_user_info", xcic_port_read_user_info},
    {"read_parameter_property", xcic_port_read_parameter_property},
    {"close", xcic_port_close},
    {"usable", xcic_port_usable},
    {"__tostring", xcic_port_to_string},
    {"__gc", xcic_port_gc},
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
	luaL_newmetatable(L, XCIC_PORT_LUA_UDATA_NAME);
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
