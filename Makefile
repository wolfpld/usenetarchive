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
	sort \
	tbrowser \
	threadify \
	utf8ize \
	verify

QTOOLS := Browser

DISPATCH := uat

TARGET := $(addprefix bin/,$(TOOLS)) $(addprefix bin/,$(QTOOLS)) $(addprefix bin/,$(DISPATCH))

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
	$(foreach dir,$(DISPATCH),make clean -C $(dir)/build/unix;)
	make clean -C browser || /bin/true

install: $(TARGET)
	install -d $(DESTDIR)/usr/{bin,lib/uat,share/man/man1}
	$(foreach util,$(DISPATCH),install -s bin/$(util) $(DESTDIR)/usr/bin/;)
	$(foreach util,$(TOOLS),install -s bin/$(util) $(DESTDIR)/usr/lib/uat/;)
	$(foreach util,$(QTOOLS),install -s bin/$(util) $(DESTDIR)/usr/lib/uat/;)
	install -m644 man/*.1 $(DESTDIR)/usr/share/man/man1/

.PHONY: all clean install
.NOTPARALLEL: $(TARGET)
.SUFFIX:
.FORCE:
