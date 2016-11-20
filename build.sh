#!/bin/sh

mkdir -p bin

for i in connectivity export-messages extract-msgid extract-msgmeta filter-newsgroups filter-spam import-source-mbox import-source-slrnpull import-source-slrnpull-7z kill-duplicates lexdist lexhash lexicon lexopt lexsort lexstats merge-raw package query query-raw relative-complement repack-lz4 repack-zstd tbrowser threadify utf8ize verify
do
    make -j4 -C $i/build/unix release
    cp -f $i/build/unix/$i bin/
done
