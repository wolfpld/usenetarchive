TOOLS := \
	connectivity \
	export-messages \
	extract-msgid \
	extract-msgmeta \
	filter-newsgroups \
	filter-spam \
	google-groups \
	import-source-maildir \
	import-source-maildir-7z \
	import-source-mbox \
	kill-duplicates \
	lexdist \
	lexhash \
	lexicon \
	lexopt \
	lexsort \
	lexstats \
	merge-raw \
	package \
	query \
	query-raw \
	relative-complement \
	repack-lz4 \
	repack-zstd \
	tbrowser \
	threadify \
	utf8ize \
	verify

TARGET := $(addprefix bin/,$(TOOLS))

all: $(TARGET)

$(TARGET): .FORCE
	$(eval base := $(shell basename $@))
	@mkdir -p bin/
	+make -C $(base)/build/unix release
	cp -f $(base)/build/unix/$(base) $@

clean:
	rm -f bin/*
	$(foreach dir,$(TOOLS),make clean -C $(dir)/build/unix;)

.PHONY: all clean
.NOTPARALLEL: $(TARGET)
.SUFFIX:
.FORCE:
