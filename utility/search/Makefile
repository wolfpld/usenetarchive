CFLAGS := -g3 -Wall
CXXFLAGS := $(CFLAGS) -std=c++14
DEFINES +=
INCLUDES := -I../../contrib/zstd/common -I../../contrib/zstd
LIBS :=
IMAGE := search

SRC := \
    search.cpp \
    ../../libuat/Archive.cpp \
    ../../libuat/PackageAccess.cpp \
    ../../common/LexiconTypes.cpp
SRC2 := \
    ../../contrib/zstd/common/entropy_common.c \
    ../../contrib/zstd/common/error_private.c \
    ../../contrib/zstd/common/fse_decompress.c \
    ../../contrib/zstd/common/xxhash.c \
    ../../contrib/zstd/common/zstd_common.c \
    ../../contrib/zstd/decompress/huf_decompress.c \
    ../../contrib/zstd/decompress/zstd_decompress.c
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
