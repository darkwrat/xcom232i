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

#define FUSE_USE_VERSION 32
#include <fuse_lowlevel.h>

#include <stdarg.h>


/////////////////////////////////////////////////////////////////////////////

static const char *hello_str = "Hello World!\n";
static const char *hello_name = "hello";

static int hello_stat(fuse_ino_t ino, struct stat *stbuf)
{
	stbuf->st_ino = ino;
	switch (ino) {
	case 1:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;

	case 2:
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
		break;

	default:
		return -1;
	}
	return 0;
}

static void hello_ll_getattr(fuse_req_t req, fuse_ino_t ino,
			     struct fuse_file_info *fi)
{
	struct stat stbuf;

	(void) fi;

	memset(&stbuf, 0, sizeof(stbuf));
	if (hello_stat(ino, &stbuf) == -1)
		fuse_reply_err(req, ENOENT);
	else
		fuse_reply_attr(req, &stbuf, 1.0);
}

static void hello_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;

	if (parent != 1 || strcmp(name, hello_name) != 0)
		fuse_reply_err(req, ENOENT);
	else {
		memset(&e, 0, sizeof(e));
		e.ino = 2;
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		hello_stat(e.ino, &e.attr);

		fuse_reply_entry(req, &e);
	}
}

struct dirbuf {
	char *p;
	size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
		       fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
			  b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
			     off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off,
				      min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

static void hello_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			     off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	if (ino != 1)
		fuse_reply_err(req, ENOTDIR);
	else {
		struct dirbuf b;

		memset(&b, 0, sizeof(b));
		dirbuf_add(req, &b, ".", 1);
		dirbuf_add(req, &b, "..", 1);
		dirbuf_add(req, &b, hello_name, 2);
		reply_buf_limited(req, b.p, b.size, off, size);
		free(b.p);
	}
}

static void hello_ll_open(fuse_req_t req, fuse_ino_t ino,
			  struct fuse_file_info *fi)
{
	if (ino != 2)
		fuse_reply_err(req, EISDIR);
	else if ((fi->flags & 3) != O_RDONLY)
		fuse_reply_err(req, EACCES);
	else
		fuse_reply_open(req, fi);
}

static void hello_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	assert(ino == 2);
	reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}

static struct fuse_lowlevel_ops hello_ll_oper = {
	.lookup		= hello_ll_lookup,
	.getattr	= hello_ll_getattr,
	.readdir	= hello_ll_readdir,
	.open		= hello_ll_open,
	.read		= hello_ll_read,
};

////////////////////////////////////////////////////////////////////////////////////////////


#define FSC_FS_LUA_UDATA_NAME "__tnt_fsc_fs"

LUA_API int luaopen_fsc(lua_State *L);

struct fsc_fs {
	struct fuse_session *se;
	struct fiber *fib;
};

static int fsc_new_fs(lua_State *L);

static int fsc_fs_mount(lua_State *L);
static int fsc_fs_run_loop(lua_State *L);
static int fsc_fs_exit(lua_State *L);
static int fsc_fs_unmount(lua_State *L);
static int fsc_fs_gc(lua_State *L);

static int fsc_intl_fs_loop_fiber_func(va_list ap);

#define fsc_lua_except_to(label, L, ...)                                                           \
	({                                                                                         \
		(void)lua_pushfstring(L, __VA_ARGS__);                                             \
		goto label;                                                                        \
	})

#define fsc_lua_except(L, ...) fsc_lua_except_to(except, L, __VA_ARGS__)

int fsc_new_fs(lua_State *L)
{
	struct fsc_fs *fs = (struct fsc_fs *)lua_newuserdata(L, sizeof(*fs));

	memset(fs, 0, sizeof(*fs));

	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

	fuse_opt_add_arg(&args, "");
	fuse_opt_add_arg(&args, "-odebug");
	fuse_opt_add_arg(&args, "-onoatime");
	fuse_opt_add_arg(&args, "-oauto_unmount");
	fuse_opt_add_arg(&args, "-oallow_other");
	fuse_opt_add_arg(&args, "-oallow_root");

	struct fuse_lowlevel_ops op = hello_ll_oper;

	fs->se = fuse_session_new(&args, &op, sizeof(op), (void *)fs);
	if (!fs->se)
		fsc_lua_except(L, "fuse_session_new failed");

	luaL_getmetatable(L, FSC_FS_LUA_UDATA_NAME);
	lua_setmetatable(L, -2);

	return 1;

except:
	return lua_error(L);
}

int fsc_fs_mount(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Usage: fs:mount(mountpoint)");

	struct fsc_fs *fs = (struct fsc_fs *)luaL_checkudata(L, 1, FSC_FS_LUA_UDATA_NAME);

	if (fuse_session_mount(fs->se, lua_tostring(L, 2)))
		fsc_lua_except(L, "fuse_session_mount failed");

	return 0;

except:
	return lua_error(L);
}

int fsc_fs_run_loop(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: fs:run_loop()");

	struct fsc_fs *fs = (struct fsc_fs *)luaL_checkudata(L, 1, FSC_FS_LUA_UDATA_NAME);

	fs->fib = fiber_new("fsc_fuse_loop", fsc_intl_fs_loop_fiber_func);
	fiber_start(fs->fib, fs);

	return 0;
}

int fsc_fs_exit(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: fs:exit()");

	struct fsc_fs *fs = (struct fsc_fs *)luaL_checkudata(L, 1, FSC_FS_LUA_UDATA_NAME);

	fuse_session_exit(fs->se);

	return 0;
}

int fsc_fs_unmount(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Usage: fs:unmount()");

	struct fsc_fs *fs = (struct fsc_fs *)luaL_checkudata(L, 1, FSC_FS_LUA_UDATA_NAME);

	fuse_session_unmount(fs->se);

	return 0;
}

int fsc_fs_gc(lua_State *L) { return 0; }

int fsc_intl_fs_loop_fiber_func(va_list ap)
{
	struct fsc_fs *fs = va_arg(ap, struct fsc_fs *);

	int res = 0;
	struct fuse_buf fbuf = {
	    .mem = NULL,
	};

	while (!fuse_session_exited(fs->se)) {
		int fd = fuse_session_fd(fs->se);
		if (fd == -1)
			break;

		int w = coio_wait(fd, COIO_READ, 100);
		if (fuse_session_exited(fs->se) || fiber_is_cancelled())
			break;
		else if (!(w & COIO_READ))
			continue;

		res = fuse_session_receive_buf(fs->se, &fbuf);
		if (res == -EINTR)
			continue;
		if (res <= 0)
			break;

		fuse_session_process_buf(fs->se, &fbuf);
	}

	free(fbuf.mem);
	fuse_session_reset(fs->se);

	return 0;
}

/*
 * List of exporting: aliases, callbacks, definitions, functions etc [[
 */
struct define {
	const char *name;
	int value;
};

static const struct define defines[] = {{"FUSE_USE_VERSION", FUSE_USE_VERSION}, {NULL, 0}};

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {{"new_fs", fsc_new_fs}, {NULL, NULL}};

static const struct luaL_Reg M[] = {{"mount", fsc_fs_mount}, {"run_loop", fsc_fs_run_loop},
				    {"exit", fsc_fs_exit},   {"unmount", fsc_fs_unmount},
				    {"__gc", fsc_fs_gc},     {NULL, NULL}};
/*
 * ]]
 */

/*
 * Lib initializer
 */
LUA_API int luaopen_fsc(lua_State *L)
{
	/**
	 * Add metatable.__index = metatable
	 */
	luaL_newmetatable(L, FSC_FS_LUA_UDATA_NAME);
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
