# NB: this Makefile is for the "I just want to build this" crowd. If you want
# to actually tinker with the code or build it more than once it will be a bad
# fit for you, since there is no dependency handling for header files. I just
# find that part of GNU make to be a terrible kludge. Also, there are no rules
# here to build performance tests and other cruft. You need to use tup for all
# of that.

CXXFLAGS = -std=c++11 -Wall -Wextra -O3 -g -Ireckless/include

target = reckless/lib/libreckless.a
srcpath = reckless/src
objects := $(patsubst %.cpp,%.o,$(wildcard $(srcpath)/*.cpp))

.PHONY: clean

$(target): $(objects)
	-$(RM) $(target)
	ar rs $(target) $(objects)

clean:
	-$(RM) $(target)
	$(RM) $(srcpath)/*.o
