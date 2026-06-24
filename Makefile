ifndef order
$(error "order not defined")
endif

CXX   	     	     := g++
CXXFLAGS     	     := -std=c++23 -Wall -Wextra -Werror

CPPTRACE_DIR 	     := cpptrace
CPPTRACE_BUILDDIR    := $(CPPTRACE_DIR)/build
CPPTRACE_LIB		 := $(CPPTRACE_BUILDDIR)/libcpptrace.a
CPPTRACE_LDFLAGS	 := -L$(CPPTRACE_BUILDDIR) -ldwarf -lcpptrace
CPPTRACE_INCLUDE     := -I$(CPPTRACE_DIR)/include

$(CPPTRACE_LIB):
	mkdir -p $(CPPTRACE_BUILDDIR)
	cmake -S $(CPPTRACE_DIR) -B $(CPPTRACE_BUILDDIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -GNinja
	cmake --build $(CPPTRACE_BUILDDIR)

cpptrace: $(CPPTRACE_LIB)

soka: 	  soka.cpp cpptrace
	$(CXX) $(CXXFLAGS) \
		-DORDER=$(order) \
		$(CPPTRACE_INCLUDE) \
		$< \
		-o $@ \
		$(CPPTRACE_LDFLAGS)

debug: 	  CXXFLAGS += -O0 -g
debug:	  BUILD_TYPE := Debug
debug: 	  soka

release:  CXXFLAGS += -O3 -DNOTHROW
release:  BUILD_TYPE := Release
release:  soka
