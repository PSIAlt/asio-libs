SRCDIR = src/
LIBDIR = lib/
OBJDIR = obj/
TESTDIR = unit_tests/

UNAME := $(shell uname)
CXX = g++
CC = gcc
SEDFLAG = -r
ifeq ($(UNAME), FreeBSD)
	CXX = clang
	CC = clang
	SEDFLAG = -E
endif
ifeq ($(UNAME), Darwin)
	CXX = clang++
	CC = clang
	SEDFLAG = -E
endif

CFLAGS = $(ADD_CXXFLAGS) -Wall -O2 -ggdb3 -std=c99 -I $(LIBDIR)
CXXFLAGS = $(ADD_CXXFLAGS) -std=c++11 -Wall -I /usr/include/ -I /usr/local/include/ -I $(LIBDIR) -O2 -I /usr/local/lib/gcc47/include/ -ggdb3
LDFLAGS = -L /usr/local/lib/ -L /usr/lib/ -lboost_system -lboost_thread-mt -lboost_coroutine-mt
SRCST = $(wildcard $(SRCDIR)*.cpp)
SRCS = $(SRCST:$(SRCDIR)%=%)
OBJS = $(SRCS:.cpp=.o)
SRC_TEST_ST = $(wildcard $(TESTDIR)*.cpp)
SRC_TEST = $(SRC_TEST_ST:$(TESTDIR)%=%)
OBJS_TEST = $(SRC_TEST:.cpp=.test)

OBJS += picohttpparser.o

all: depend $(addprefix $(OBJDIR), $(OBJS))

$(OBJDIR)picohttpparser.o: $(SRCDIR)picohttpparser.c
	$(CC) $(CFLAGS) -c -o $@ $< 

tests: $(addprefix $(TESTDIR), main.cpp) $(addprefix $(OBJDIR), $(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(OBJDIR)unit_tests $^

$(OBJDIR)%.o: $(SRCDIR)%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

depend: $(addprefix $(SRCDIR),$(SRCS)) $(wildcard $(LIBDIR)*.h)
	$(CXX) -MM $(CXXFLAGS) $(addprefix $(SRCDIR),$(SRCS)) | sed $(SEDFLAG) 's/^([^ ])/obj\/\1/' > depend

-include depend

cppcheck:
	cppcheck --enable=all -I $(LIBDIR) $(SRCDIR)*.cpp $(LIBDIR)*.h $(LIBDIR)*.hpp

test: all $(addprefix $(OBJDIR), $(OBJS_TEST))
	for i in $(OBJS_TEST); do \
		./$(OBJDIR)$$i ; \
	done

$(OBJDIR)%.test: $(TESTDIR)%.cpp $(addprefix $(OBJDIR), $(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(addprefix $(OBJDIR), $(OBJS))

.PHONY: clean
clean:
	rm -f depend $(OBJDIR)*.o $(OBJDIR)/unit_tests
