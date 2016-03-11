UNAME := $(shell uname -s)
SHLIB := libi2pd.so
ARLIB := libi2pd.a
SHLIB_CLIENT := libi2pdclient.so
ARLIB_CLIENT := libi2pdclient.a
I2PD  := i2pd
GREP := fgrep
DEPS := obj/make.dep

include filelist.mk

USE_AESNI  := yes
USE_STATIC := no

ifeq ($(UNAME),Darwin)
	DAEMON_SRC += DaemonLinux.cpp
	include Makefile.osx
else ifeq ($(shell echo $(UNAME) | $(GREP) -c FreeBSD),1)
	DAEMON_SRC += DaemonLinux.cpp
	include Makefile.bsd
else ifeq ($(UNAME),Linux)
	DAEMON_SRC += DaemonLinux.cpp
	include Makefile.linux
else # win32 mingw
	DAEMON_SRC += DaemonWin32.cpp Win32/Win32Service.cpp Win32/Win32App.cpp
	include Makefile.mingw
endif

all: mk_obj_dir $(ARLIB) $(ARLIB_CLIENT) $(I2PD)

mk_obj_dir:
	@mkdir -p obj
	@mkdir -p obj/Win32

api: mk_obj_dir $(SHLIB) $(ARLIB)
api_client: mk_obj_dir $(SHLIB) $(ARLIB) $(SHLIB_CLIENT) $(ARLIB_CLIENT)

## NOTE: The NEEDED_CXXFLAGS are here so that CXXFLAGS can be specified at build time
## **without** overwriting the CXXFLAGS which we need in order to build.
## For example, when adding 'hardening flags' to the build
## (e.g. -fstack-protector-strong -Wformat -Werror=format-security), we do not want to remove
## -std=c++11. If you want to remove this variable please do so in a way that allows setting
## custom FLAGS to work at build-time.

deps: mk_obj_dir
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) -MM *.cpp > $(DEPS)
	@sed -i -e '/\.o:/ s/^/obj\//' $(DEPS)

obj/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) $(CPU_FLAGS) -c -o $@ $<

# '-' is 'ignore if missing' on first run
-include $(DEPS)

DAEMON_OBJS += $(patsubst %.cpp,obj/%.o,$(DAEMON_SRC))
$(I2PD): $(DAEMON_OBJS) $(ARLIB) $(ARLIB_CLIENT)
	$(CXX) -o $@ $^ $(LDLIBS) $(LDFLAGS)

$(SHLIB): $(patsubst %.cpp,obj/%.o,$(LIB_SRC))
ifneq ($(USE_STATIC),yes)
	$(CXX) $(LDFLAGS) $(LDLIBS) -shared -o $@ $^
endif

$(SHLIB_CLIENT): $(patsubst %.cpp,obj/%.o,$(LIB_CLIENT_SRC))
	$(CXX) $(LDFLAGS) $(LDLIBS) -shared -o $@ $^

$(ARLIB): $(patsubst %.cpp,obj/%.o,$(LIB_SRC))
	ar -r $@ $^

$(ARLIB_CLIENT): $(patsubst %.cpp,obj/%.o,$(LIB_CLIENT_SRC))
	ar -r $@ $^

clean:
	rm -rf obj
	$(RM) $(I2PD) $(SHLIB) $(ARLIB) $(SHLIB_CLIENT) $(ARLIB_CLIENT)

strip: $(I2PD) $(SHLIB_CLIENT) $(SHLIB)
	strip $^

LATEST_TAG=$(shell git describe --tags --abbrev=0 master)
dist:
	git archive --format=tar.gz -9 --worktree-attributes \
	    --prefix=i2pd_$(LATEST_TAG)/ $(LATEST_TAG) -o i2pd_$(LATEST_TAG).tar.gz

.PHONY: all
.PHONY: clean
.PHONY: deps
.PHONY: dist
.PHONY: api
.PHONY: api_client
.PHONY: mk_obj_dir
