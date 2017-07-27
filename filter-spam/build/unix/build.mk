CFLAGS += 
CXXFLAGS := $(CFLAGS) -std=c++14
DEFINES +=
INCLUDES := -I../../../contrib -I../../../contrib/zstd/common -I../../../contrib/zstd
LIBS :=
IMAGE := filter-spam

SRC := $(shell egrep 'ClCompile.*cpp"' ../win32/$(IMAGE).vcxproj | sed -e 's/.*\"\(.*\)\".*/\1/' | sed -e 's@\\@/@g')
SRC2 := $(shell egrep 'ClCompile.*cc"' ../win32/$(IMAGE).vcxproj | sed -e 's/.*\"\(.*\)\".*/\1/' | sed -e 's@\\@/@g')
OBJ := $(SRC:%.cpp=%.o)
OBJ2 := $(SRC2:%.cc=%.o)

all: $(IMAGE)

%.o: %.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $(DEFINES) $< -o $@

%.d : %.cpp
	@echo Resolving dependencies of $<
	@mkdir -p $(@D)
	@$(CXX) -MM $(INCLUDES) $(CXXFLAGS) $(DEFINES) $< > $@.$$$$; \
	sed 's,.*\.o[ :]*,$(<:.cpp=.o) $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cc
	$(CXX) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@

%.d : %.cc
	@echo Resolving dependencies of $<
	@mkdir -p $(@D)
	@$(CXX) -MM $(INCLUDES) $(CFLAGS) $(DEFINES) $< > $@.$$$$; \
	sed 's,.*\.o[ :]*,$(<:.cc=.o) $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(IMAGE): $(OBJ) $(OBJ2)
	$(CXX) $(CXXFLAGS) $(DEFINES) $(OBJ) $(OBJ2) $(LIBS) -o $@

ifneq "$(MAKECMDGOALS)" "clean"
-include $(SRC:.cpp=.d) $(SRC2:.cc=.d)
endif

clean:
	rm -f $(OBJ) $(OBJ2) $(SRC:.cpp=.d) $(SRC2:.cc=.d) $(IMAGE)

.PHONY: clean all
