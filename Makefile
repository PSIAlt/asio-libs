SRCDIR = src/
LIBDIR = lib/
OBJDIR = obj/

APPNAME = asio-libs

UNAME := $(shell uname)
ifeq ($(UNAME), FreeBSD)
#	CXX = g++47
	CXX = clang
	SEDFLAG = -E
ifeq ($(UNAME), Darwin)
        CXX = clang
        SEDFLAG = -E
else
	CXX = g++
	SEDFLAG = -r
endif

CC = $(CXX)
CXXFLAGS = -Wall -I /usr/include/ -I /usr/local/include/ -I $(LIBDIR) -O2 -I /usr/local/lib/gcc47/include/ -ggdb3 
LDFLAGS = -L /usr/local/lib/gcc47/ -L /usr/local/lib/ -L /usr/lib/ -lboost_system -lboost_thread
SRCST = $(wildcard $(SRCDIR)*.cpp)
SRCS = $(SRCST:$(SRCDIR)%=%)
OBJS = $(SRCS:.cpp=.o)

all: depend $(APPNAME)

$(APPNAME): $(addprefix $(OBJDIR), $(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(OBJDIR)%.o: $(SRCDIR)%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

depend: $(addprefix $(SRCDIR),$(SRCS)) $(wildcard $(LIBDIR)*.h)
	$(CXX) -MM $(CXXFLAGS) $(addprefix $(SRCDIR),$(SRCS)) | sed $(SEDFLAG) 's/^([^ ])/obj\/\1/' > depend

-include depend

cppcheck:
	cppcheck --enable=all -I $(LIBDIR) $(SRCDIR)*.cpp $(LIBDIR)*.h $(LIBDIR)*.hpp

.PHONY: clean
clean:
	rm -f $(APPNAME) depend $(OBJDIR)*.o
