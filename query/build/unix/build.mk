CFLAGS += 
CXXFLAGS := $(CFLAGS) -std=c++14
DEFINES +=
INCLUDES := -I../../../contrib/zstd/common -I../../../contrib/zstd
LIBS := -lpthread
IMAGE := query

SRC := $(shell egrep 'ClCompile.*cpp"' ../win32/$(IMAGE).vcxproj | sed -e 's/.*\"\(.*\)\".*/\1/' | sed -e 's@\\@/@g')
SRC2 := $(shell egrep 'ClCompile.*c"' ../win32/$(IMAGE).vcxproj | sed -e 's/.*\"\(.*\)\".*/\1/' | sed -e 's@\\@/@g')
OBJ := $(SRC:%.cpp=%.o)
OBJ2 := $(SRC2:%.c=%.o)

all: $(IMAGE)

%.o: %.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $(DEFINES) $< -o $@

%.d : %.cpp
	@echo Resolving dependencies of $<
	@mkdir -p $(@D)
	@$(CXX) -MM $(INCLUDES) $(CXXFLAGS) $(DEFINES) $< > $@.$$$$; \
	sed 's,.*\.o[ :]*,$(<:.cpp=.o) $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@

%.d : %.c
	@echo Resolving dependencies of $<
	@mkdir -p $(@D)
	@$(CC) -MM $(INCLUDES) $(CFLAGS) $(DEFINES) $< > $@.$$$$; \
	sed 's,.*\.o[ :]*,$(<:.c=.o) $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(IMAGE): $(OBJ) $(OBJ2)
	$(CXX) $(CXXFLAGS) $(DEFINES) $(OBJ) $(OBJ2) $(LIBS) -o $@

ifneq "$(MAKECMDGOALS)" "clean"
-include $(SRC:.cpp=.d) $(SRC2:.c=.d)
endif

clean:
	rm -f $(OBJ) $(OBJ2) $(SRC:.cpp=.d) $(SRC2:.c=.d) $(IMAGE)

.PHONY: clean all
