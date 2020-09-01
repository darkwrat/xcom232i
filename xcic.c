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
#include <tarantool/module.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <small/small.h>
#include <small/static.h>
#include <small/ibuf.h>

#include <scom_property.h>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define XCIC_PORT_LUA_UDATA_NAME "__tnt_xcic_port"

LUA_API int luaopen_xcic(lua_State *L);

static int xcic_open_port(lua_State *L);
static int xcic_calc_checksum(lua_State *L);

static int xcic_pack_le32(lua_State *L);
static int xcic_unpack_le32(lua_State *L);
static int xcic_pack_le16(lua_State *L);
static int xcic_unpack_le16(lua_State *L);
static int xcic_pack_le_float(lua_State *L);
static int xcic_unpack_le_float(lua_State *L);
static int xcic_pack_bool(lua_State *L);
static int xcic_unpack_bool(lua_State *L);
static int xcic_pack_signal(lua_State *L);
static int xcic_unpack_software_version(lua_State *L);

static int xcic_port_close(lua_State *L);
static int xcic_port_usable(lua_State *L);
static int xcic_port_to_string(lua_State *L);
static int xcic_port_gc(lua_State *L);
static int xcic_port_read_user_info(lua_State *L);
static int xcic_port_read_parameter_property(lua_State *L);
static int xcic_port_write_parameter_property(lua_State *L);
static int xcic_port_read_message(lua_State *L);

/** Xcom-232i serial port handle. */
struct xcic_port {
	/** The file descriptor of the opened serial port. */
	int fd;
	/** Latch for mutual exclusion of DTE exchanges. */
	box_latch_t *latch;
};

static int xcic_scom_read_property(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
				   scom_property_t *property);
static int xcic_scom_write_property(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
				    scom_property_t *property, const char *data, size_t data_len);
static int xcic_scom_port_exchange(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
				   scom_frame_t *frame);

static int xcic_scom_encode_read_property(lua_State *L, struct ibuf *ibuf,
					  scom_property_t *property);
static int xcic_scom_encode_write_property(lua_State *L, struct ibuf *ibuf,
					   scom_property_t *property, const char *data,
					   size_t data_len);
static int xcic_scom_encode_request_frame(lua_State *L, struct ibuf *ibuf, scom_frame_t *frame);
static int xcic_scom_decode_frame_header(lua_State *L, scom_frame_t *frame);
static int xcic_scom_decode_frame_data(lua_State *L, scom_frame_t *frame);
static int xcic_scom_decode_read_property(lua_State *L, scom_property_t *property);
static int xcic_scom_decode_write_property(lua_State *L, scom_property_t *property);

static uint16_t xcic_scom_calc_checksum(const char *data, uint_fast16_t length);
static void xcic_scom_dump_faulty_frame(struct ibuf *ibuf, scom_frame_t *frame);
static char *xcic_scom_strerror(scom_error_t error);

static ssize_t xcic_intl_port_read(struct xcic_port *xp, void *buf, size_t count);
static ssize_t xcic_intl_port_write(struct xcic_port *xp, void *buf, size_t count);

static void xcic_intl_port_close(struct xcic_port *xp);

static ssize_t xcic_intl_open_cb(va_list ap);

#define xcic_lua_except_to(label, L, ...)                                                          \
	({                                                                                         \
		(void)lua_pushfstring(L, __VA_ARGS__);                                             \
		goto label;                                                                        \
	})

#define xcic_lua_except(L, ...) xcic_lua_except_to(except, L, __VA_ARGS__)

int xcic_open_port(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.open_port(pathname)");

	struct xcic_port *xp = (struct xcic_port *)lua_newuserdata(L, sizeof(*xp));

	memset(xp, 0, sizeof(*xp));

	const char *pathname = lua_tostring(L, 1);

	xp->fd = coio_call(xcic_intl_open_cb, pathname, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (xp->fd == -1)
		xcic_lua_except(L, "open: %s", strerror(errno));

	struct termios tty = {.c_cflag = CS8 | CLOCAL | CREAD | PARENB};

	(void)cfsetospeed(&tty, B38400);
	(void)cfsetispeed(&tty, B38400);

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

int xcic_calc_checksum(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.calc_checksum(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	lua_pushinteger(L, xcic_scom_calc_checksum(data, (uint16_t)data_len));

	return 1;
}

int xcic_port_close(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:close()");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	xcic_intl_port_close(xp);

	return 0;
}

int xcic_port_usable(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:usable()");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	lua_pushboolean(L, xp->fd != -1);

	return 1;
}

int xcic_port_to_string(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xp:tostring()");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	lua_pushfstring(L, "xcic_port: %p (%d)", xp, xp->fd);

	return 1;
}

int xcic_port_gc(lua_State *L)
{
	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	xcic_intl_port_close(xp);

	box_latch_delete(xp->latch);

	return 0;
}

int xcic_port_read_user_info(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L, "Usage: xp:read_user_info(dst_addr, object_id)");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t frame;
	scom_initialize_frame(&frame, NULL, 0);

	frame.src_addr = 1;
	frame.dst_addr = lua_tointeger(L, 2);

	scom_property_t property;
	scom_initialize_property(&property, &frame);

	property.object_type = SCOM_USER_INFO_OBJECT_TYPE;
	property.object_id = lua_tointeger(L, 3);
	property.property_id = 1;

	struct ibuf ibuf __attribute__((cleanup(ibuf_destroy))) = {0};
	ibuf_create(&ibuf, cord_slab_cache(), 32);

	if (xcic_scom_read_property(L, xp, &ibuf, &property))
		goto except;

	lua_pushlstring(L, property.value_buffer, property.value_length);

	return 1;

except:
	return lua_error(L);
}

int xcic_port_read_parameter_property(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L, "Usage: xp:read_parameter_property(dst_addr, "
				     "object_id, property_id)");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t frame;
	scom_initialize_frame(&frame, NULL, 0);

	frame.src_addr = 1;
	frame.dst_addr = lua_tointeger(L, 2);

	scom_property_t property;
	scom_initialize_property(&property, &frame);

	property.object_type = SCOM_PARAMETER_OBJECT_TYPE;
	property.object_id = lua_tointeger(L, 3);
	property.property_id = lua_tointeger(L, 4);

	struct ibuf ibuf __attribute__((cleanup(ibuf_destroy))) = {0};
	ibuf_create(&ibuf, cord_slab_cache(), 32);

	if (xcic_scom_read_property(L, xp, &ibuf, &property))
		goto except;

	lua_pushlstring(L, property.value_buffer, property.value_length);

	return 1;

except:
	return lua_error(L);
}

int xcic_scom_read_property(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
			    scom_property_t *property)
{
	uint32_t object_id = property->object_id;

	if (xcic_scom_encode_read_property(L, ibuf, property))
		goto except;

	if (xcic_scom_encode_request_frame(L, ibuf, property->frame))
		goto except;

	if (xcic_scom_port_exchange(L, xp, ibuf, property->frame))
		goto except;

	if (xcic_scom_decode_frame_data(L, property->frame)) {
		xcic_scom_dump_faulty_frame(ibuf, property->frame);
		goto except;
	}

	scom_initialize_property(property, property->frame);

	if (xcic_scom_decode_read_property(L, property))
		goto except;

	if (object_id != property->object_id)
		xcic_lua_except(L, "mismatch on object_id `%d` != `%d`", property->object_id,
				object_id);

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_port_write_parameter_property(lua_State *L)
{
	if (lua_gettop(L) < 4)
		return luaL_error(L, "Usage: xp:write_parameter_property(dst_addr, "
				     "object_id, property_id, data)");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t frame;
	scom_initialize_frame(&frame, NULL, 0);

	frame.src_addr = 1;
	frame.dst_addr = lua_tointeger(L, 2);

	scom_property_t property;
	scom_initialize_property(&property, &frame);

	property.object_type = SCOM_PARAMETER_OBJECT_TYPE;
	property.object_id = lua_tointeger(L, 3);
	property.property_id = lua_tointeger(L, 4);

	size_t data_len;
	const char *data = lua_tolstring(L, 5, &data_len);

	struct ibuf ibuf __attribute__((cleanup(ibuf_destroy))) = {0};
	ibuf_create(&ibuf, cord_slab_cache(), 32);

	if (xcic_scom_write_property(L, xp, &ibuf, &property, data, data_len))
		goto except;

	return 0;

except:
	return lua_error(L);
}

int xcic_scom_write_property(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
			     scom_property_t *property, const char *data, size_t data_len)
{
	uint32_t object_id = property->object_id;

	if (xcic_scom_encode_write_property(L, ibuf, property, data, data_len))
		goto except;

	if (xcic_scom_encode_request_frame(L, ibuf, property->frame))
		goto except;

	if (xcic_scom_port_exchange(L, xp, ibuf, property->frame))
		goto except;

	if (xcic_scom_decode_frame_data(L, property->frame)) {
		xcic_scom_dump_faulty_frame(ibuf, property->frame);
		goto except;
	}

	scom_initialize_property(property, property->frame);

	if (xcic_scom_decode_write_property(L, property))
		goto except;

	if (object_id != property->object_id)
		xcic_lua_except(L, "mismatch on object_id `%d` != `%d`", property->object_id,
				object_id);

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_port_read_message(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L, "Usage: xp:read_message(dst_addr, object_id)");

	struct xcic_port *xp = (struct xcic_port *)luaL_checkudata(L, 1, XCIC_PORT_LUA_UDATA_NAME);

	scom_frame_t frame;
	scom_initialize_frame(&frame, NULL, 0);

	frame.src_addr = 1;
	frame.dst_addr = lua_tointeger(L, 2);

	scom_property_t property;
	scom_initialize_property(&property, &frame);

	property.object_type = 3; // message
	property.object_id = lua_tointeger(L, 3);
	property.property_id = 0;

	struct ibuf ibuf __attribute__((cleanup(ibuf_destroy))) = {0};
	ibuf_create(&ibuf, cord_slab_cache(), 32);

	if (xcic_scom_read_property(L, xp, &ibuf, &property))
		goto except;

	if (property.value_length != 4 + 2 + 4 + 4 + 4)
		xcic_lua_except(L, "invalid message data length %d", property.value_length);

	lua_pushinteger(L, scom_read_le32(&property.value_buffer[0]));
	lua_pushinteger(L, scom_read_le16(&property.value_buffer[4]));
	lua_pushinteger(L, scom_read_le32(&property.value_buffer[6]));
	lua_pushinteger(L, scom_read_le32(&property.value_buffer[10]));
	lua_pushinteger(L, scom_read_le32(&property.value_buffer[14]));

	return 5;

except:
	return lua_error(L);
}

int xcic_scom_port_exchange(lua_State *L, struct xcic_port *xp, struct ibuf *ibuf,
			    scom_frame_t *frame)
{
	ssize_t nb;

	uint32_t dst_addr = frame->dst_addr;

	box_latch_lock(xp->latch);

	nb = xcic_intl_port_write(xp, frame->buffer, scom_frame_length(frame));

	if (nb != (ssize_t)scom_frame_length(frame))
		xcic_lua_except(L, "error when writing to the com port");

	ibuf_reset(ibuf);

	if (!ibuf_alloc(ibuf, SCOM_FRAME_HEADER_SIZE))
		xcic_lua_except(L, "alloc failed");

	scom_initialize_frame(frame, ibuf->rpos, ibuf_used(ibuf));

	nb = xcic_intl_port_read(xp, frame->buffer, SCOM_FRAME_HEADER_SIZE);

	if (nb != SCOM_FRAME_HEADER_SIZE)
		xcic_lua_except(L, "error when reading the header from the com port");

	/* scom_frame_length() is incorrect as `frame->data_length` is still
	 * empty */
	ssize_t rlen = scom_read_le16(&frame->buffer[10]) + 2;

	if (!ibuf_alloc(ibuf, rlen))
		xcic_lua_except(L, "alloc failed");

	frame->buffer = ibuf->rpos;
	frame->buffer_size = ibuf_used(ibuf);

	if (xcic_scom_decode_frame_header(L, frame))
		goto except;

	nb = xcic_intl_port_read(xp, &frame->buffer[SCOM_FRAME_HEADER_SIZE], rlen);

	if (nb != rlen)
		xcic_lua_except(L, "error when reading the data from the com port");

	if (dst_addr != frame->src_addr)
		xcic_lua_except(L, "mismatch on address `%d` != `%d`", dst_addr, frame->dst_addr);

	box_latch_unlock(xp->latch);

	return 0;

except:
	xcic_intl_port_close(xp);
	box_latch_unlock(xp->latch);

	return -1; // caller must invoke `lua_error`
}

uint16_t xcic_scom_calc_checksum(const char *data, uint_fast16_t length)
{
	uint_fast8_t A = 0xFF, B = 0;

	while (length--) {
		A = (A + *data++) & 0xFF;
		B = (B + A) & 0xFF;
	}

	return (B & 0xFF) << 8 | (A & 0xFF);
}

void xcic_scom_dump_faulty_frame(struct ibuf *ibuf, scom_frame_t *frame)
{
	size_t len = scom_frame_length(frame);
	char *esc = (char *)static_reserve(len * 4 + 1);

	if (esc) {
		char *ptr = esc;
		for (size_t i = 0; i < scom_frame_length(frame); i++)
			ptr += sprintf(ptr, "\\%d", (uint8_t)frame->buffer[i]);

		*ptr = '\0';
	}

	uint16_t cs =
	    xcic_scom_calc_checksum(&frame->buffer[SCOM_FRAME_HEADER_SIZE], frame->data_length);

	say_info("xcic: faulty frame len %d iu %d cs %d esc %s", len, ibuf_used(ibuf), cs,
		 esc ?: "-");
}

int xcic_scom_encode_read_property(lua_State *L, struct ibuf *ibuf, scom_property_t *property)
{
	if (!ibuf_alloc(ibuf, SCOM_FRAME_HEADER_SIZE + 2 + 8))
		xcic_lua_except(L, "alloc failed");

	property->frame->buffer = ibuf->rpos;
	property->frame->buffer_size = ibuf_used(ibuf);

	scom_encode_read_property(property);

	if (property->frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "read property frame encoding failed with error %d (%s)",
				property->frame->last_error,
				xcic_scom_strerror(property->frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_encode_write_property(lua_State *L, struct ibuf *ibuf, scom_property_t *property,
				    const char *data, size_t data_len)
{
	size_t offset = SCOM_FRAME_HEADER_SIZE + 2 + 8;

	if (!ibuf_alloc(ibuf, offset + data_len))
		xcic_lua_except(L, "alloc failed");

	memcpy(ibuf->rpos + offset, data, data_len);

	property->frame->buffer = ibuf->rpos;
	property->frame->buffer_size = ibuf_used(ibuf);
	property->value_length = data_len;

	scom_encode_write_property(property);

	if (property->frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "write property frame encoding failed with error %d (%s)",
				property->frame->last_error,
				xcic_scom_strerror(property->frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_encode_request_frame(lua_State *L, struct ibuf *ibuf, scom_frame_t *frame)
{
	if (!ibuf_alloc(ibuf, scom_frame_length(frame) - frame->buffer_size))
		xcic_lua_except(L, "alloc failed");

	frame->buffer = ibuf->rpos;
	frame->buffer_size = ibuf_used(ibuf);

	scom_encode_request_frame(frame);

	if (frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "data link frame encoding failed with error %d (%s)",
				frame->last_error, xcic_scom_strerror(frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_decode_frame_header(lua_State *L, scom_frame_t *frame)
{
	scom_decode_frame_header(frame);

	if (frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "data link header decoding failed with error %d (%s)",
				frame->last_error, xcic_scom_strerror(frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_decode_frame_data(lua_State *L, scom_frame_t *frame)
{
	scom_decode_frame_data(frame);

	if (frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "data link data decoding failed with error %d (%s)",
				frame->last_error, xcic_scom_strerror(frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_decode_read_property(lua_State *L, scom_property_t *property)
{
	scom_decode_read_property(property);

	if (property->frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "read property decoding failed with error %d (%s)",
				property->frame->last_error,
				xcic_scom_strerror(property->frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

int xcic_scom_decode_write_property(lua_State *L, scom_property_t *property)
{
	scom_decode_write_property(property);

	if (property->frame->last_error != SCOM_ERROR_NO_ERROR)
		xcic_lua_except(L, "write property decoding failed with error %d (%s)",
				property->frame->last_error,
				xcic_scom_strerror(property->frame->last_error));

	return 0;

except:
	return -1; // caller must invoke `lua_error`
}

char *xcic_scom_strerror(scom_error_t error)
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

int xcic_pack_le32(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.pack_le32(val)");

	uint32_t val;
	scom_write_le32((char *const) & val, (uint32_t)lua_tointeger(L, 1));

	lua_pushlstring(L, (const char *)&val, 4);

	return 1;
}

int xcic_unpack_le32(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le32(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		xcic_lua_except(L, "invalid le32 length %d", data_len);

	lua_pushinteger(L, scom_read_le32(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_pack_le16(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.pack_le16(val)");

	uint16_t val;
	scom_write_le16((char *const) & val, (uint16_t)lua_tointeger(L, 1));

	lua_pushlstring(L, (const char *)&val, 2);

	return 1;
}

int xcic_unpack_le16(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le16(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 2)
		xcic_lua_except(L, "invalid le16 length %d", data_len);

	lua_pushinteger(L, scom_read_le16(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_pack_le_float(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.pack_le_float(val)");

	uint32_t val;
	scom_write_le_float((char *const) & val, (float)lua_tonumber(L, 1));

	lua_pushlstring(L, (const char *)&val, 4);

	return 1;
}

int xcic_unpack_le_float(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_le_float(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 4)
		xcic_lua_except(L, "invalid le float length %d", data_len);

	lua_pushnumber(L, (lua_Number)scom_read_le_float(data));

	return 1;

except:
	return lua_error(L);
}

int xcic_pack_bool(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.pack_bool(val)");

	uint8_t val = (uint8_t)lua_toboolean(L, 1);

	lua_pushlstring(L, (const char *)&val, 1);

	return 1;
}

int xcic_unpack_bool(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_bool(data)");

	size_t data_len;
	const char *data = lua_tolstring(L, 1, &data_len);

	if (data_len != 1)
		xcic_lua_except(L, "invalid bool length %d", data_len);

	lua_pushboolean(L, *data);

	return 1;

except:
	return lua_error(L);
}

int xcic_pack_signal(lua_State *L)
{
	uint32_t val = 1;

	lua_pushlstring(L, (const char *)&val, 4);

	return 1;
}

int xcic_unpack_software_version(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: xcic.unpack_software_version(msb_data, lsb_data)");

	size_t msb_data_len;
	const char *msb_data = lua_tolstring(L, 1, &msb_data_len);

	if (msb_data_len != 4)
		xcic_lua_except(L, "invalid msb data length %d", msb_data_len);

	size_t lsb_data_len;
	const char *lsb_data = lua_tolstring(L, 2, &lsb_data_len);

	if (lsb_data_len != 4)
		xcic_lua_except(L, "invalid lsb data length %d", lsb_data_len);

	uint16_t msb = (uint16_t)scom_read_le_float(msb_data);
	uint16_t lsb = (uint16_t)scom_read_le_float(lsb_data);

	lua_pushfstring(L, "%d.%d.%d", msb >> 8, lsb >> 8, lsb & 0xFF);

	return 1;

except:
	return lua_error(L);
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

void xcic_intl_port_close(struct xcic_port *xp)
{
	int fd = xp->fd;
	xp->fd = -1;

	(void)coio_close(fd);
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

static const struct define defines[] = {{"FRAME_HEADER_SIZE", SCOM_FRAME_HEADER_SIZE}, {NULL, 0}};

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {{"open_port", xcic_open_port},
				    {"calc_checksum", xcic_calc_checksum},
				    {"pack_le32", xcic_pack_le32},
				    {"unpack_le32", xcic_unpack_le32},
				    {"pack_le16", xcic_pack_le16},
				    {"unpack_le16", xcic_unpack_le16},
				    {"pack_le_float", xcic_pack_le_float},
				    {"unpack_le_float", xcic_unpack_le_float},
				    {"pack_bool", xcic_pack_bool},
				    {"unpack_bool", xcic_unpack_bool},
				    {"pack_signal", xcic_pack_signal},
				    {"unpack_software_version", xcic_unpack_software_version},
				    {NULL, NULL}};

static const struct luaL_Reg M[] = {
    {"close", xcic_port_close},
    {"usable", xcic_port_usable},
    {"read_user_info", xcic_port_read_user_info},
    {"read_parameter_property", xcic_port_read_parameter_property},
    {"write_parameter_property", xcic_port_write_parameter_property},
    {"read_message", xcic_port_read_message},
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
