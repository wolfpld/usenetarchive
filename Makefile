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

QTOOLS := Browser

TARGET := $(addprefix bin/,$(TOOLS)) $(addprefix bin/,$(QTOOLS))

all: $(TARGET)

$(TARGET): .FORCE
	$(eval base := $(shell basename $@))
	@mkdir -p bin/
	+make -C $(base)/build/unix release
	cp -f $(base)/build/unix/$(base) $@

bin/Browser: .FORCE
	cd browser && qmake
	+make -C browser
	cp -f browser/Browser $@

clean:
	rm -f bin/*
	$(foreach dir,$(TOOLS),make clean -C $(dir)/build/unix;)
	make clean -C browser

.PHONY: all clean
.NOTPARALLEL: $(TARGET)
.SUFFIX:
.FORCE:
