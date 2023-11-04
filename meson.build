project('UsenetArchive', 'c', 'cpp', default_options: ['cpp_std=c++14'])

compiler = meson.get_compiler('cpp')
compile_args = compiler.get_supported_arguments('-march=native')
add_project_arguments(compile_args, language: ['c', 'cpp'])


common_src = [
    'common/Filesystem.cpp',
    'common/ICU.cpp',
    'common/KillRe.cpp',
    'common/LexiconTypes.cpp',
    'common/MessageLines.cpp',
    'common/MessageLogic.cpp',
    'common/mmap.cpp',
    'common/StringCompress.cpp',
    'common/System.cpp',
    'common/TaskDispatch.cpp',
    'common/UTF8.cpp',
    'contrib/xxhash/xxhash.c',
]

libuat_src = [
    'libuat/Archive.cpp',
    'libuat/Galaxy.cpp',
    'libuat/PackageAccess.cpp',
    'libuat/PersistentStorage.cpp',
    'libuat/SearchEngine.cpp',
]

zstd_src = [
    'contrib/zstd/common/debug.c',
    'contrib/zstd/common/entropy_common.c',
    'contrib/zstd/common/error_private.c',
    'contrib/zstd/common/fse_decompress.c',
    'contrib/zstd/common/pool.c',
    'contrib/zstd/common/threading.c',
    'contrib/zstd/common/xxhash.c',
    'contrib/zstd/common/zstd_common.c',
    'contrib/zstd/compress/fse_compress.c',
    'contrib/zstd/compress/hist.c',
    'contrib/zstd/compress/huf_compress.c',
    'contrib/zstd/compress/zstd_compress_literals.c',
    'contrib/zstd/compress/zstd_compress_sequences.c',
    'contrib/zstd/compress/zstd_compress_superblock.c',
    'contrib/zstd/compress/zstd_compress.c',
    'contrib/zstd/compress/zstd_double_fast.c',
    'contrib/zstd/compress/zstd_fast.c',
    'contrib/zstd/compress/zstd_lazy.c',
    'contrib/zstd/compress/zstd_ldm.c',
    'contrib/zstd/compress/zstd_opt.c',
    'contrib/zstd/compress/zstdmt_compress.c',
    'contrib/zstd/decompress/huf_decompress.c',
    'contrib/zstd/decompress/zstd_ddict.c',
    'contrib/zstd/decompress/zstd_decompress_block.c',
    'contrib/zstd/decompress/zstd_decompress.c',
    'contrib/zstd/dictBuilder/cover.c',
    'contrib/zstd/dictBuilder/divsufsort.c',
    'contrib/zstd/dictBuilder/fastcover.c',
    'contrib/zstd/dictBuilder/zdict.c',
]

lz4_src = [
    'contrib/lz4/lz4.c',
    'contrib/lz4/lz4hc.c',
]

terminator_src = [
    'contrib/kyotocabinet/kccachedb.cc',
    'contrib/kyotocabinet/kccompare.cc',
    'contrib/kyotocabinet/kccompress.cc',
    'contrib/kyotocabinet/kcdb.cc',
    'contrib/kyotocabinet/kcdbext.cc',
    'contrib/kyotocabinet/kcdirdb.cc',
    'contrib/kyotocabinet/kcfile.cc',
    'contrib/kyotocabinet/kchashdb.cc',
    'contrib/kyotocabinet/kclangc.cc',
    'contrib/kyotocabinet/kcmap.cc',
    'contrib/kyotocabinet/kcplantdb.cc',
    'contrib/kyotocabinet/kcpolydb.cc',
    'contrib/kyotocabinet/kcprotodb.cc',
    'contrib/kyotocabinet/kcregex.cc',
    'contrib/kyotocabinet/kcstashdb.cc',
    'contrib/kyotocabinet/kctextdb.cc',
    'contrib/kyotocabinet/kcthread.cc',
    'contrib/kyotocabinet/kcutil.cc',
    'contrib/terminator/terminator_classifier_base.cc',
    'contrib/terminator/terminator_classifier_bwinnow.cc',
    'contrib/terminator/terminator_classifier_hit.cc',
    'contrib/terminator/terminator_classifier_lr.cc',
    'contrib/terminator/terminator_classifier_nb.cc',
    'contrib/terminator/terminator_classifier_nsnb.cc',
    'contrib/terminator/terminator_classifier_owv.cc',
    'contrib/terminator/terminator_classifier_pa.cc',
    'contrib/terminator/terminator_classifier_pam.cc',
    'contrib/terminator/terminator_classifier_winnow.cc',
    'contrib/terminator/terminator.cc',
]

lzma_src = [
    'contrib/lzma/7zBuf.c',
    'contrib/lzma/7zBuf2.c',
    'contrib/lzma/7zCrc.c',
    'contrib/lzma/7zCrcOpt.c',
    'contrib/lzma/7zDec.c',
    'contrib/lzma/7zFile.c',
    'contrib/lzma/7zIn.c',
    'contrib/lzma/7zStream.c',
    'contrib/lzma/Bcj2.c',
    'contrib/lzma/Bra.c',
    'contrib/lzma/Bra86.c',
    'contrib/lzma/CpuArch.c',
    'contrib/lzma/Lzma2Dec.c',
    'contrib/lzma/LzmaDec.c',
    'contrib/lzma/Ppmd7.c',
    'contrib/lzma/Ppmd7Dec.c',
]

ini_src = [
    'contrib/ini/ini.c',
]

mongoose_src = [
    'contrib/mongoose/mongoose.c',
]


common_lib = static_library('common', common_src)
uat_lib = static_library('uat', libuat_src)
zstd_lib = static_library('zstd', zstd_src)
lz4_lib = static_library('lz4', lz4_src)
terminator_lib = static_library('terminator', terminator_src)
lzma_lib = static_library('lzma', lzma_src)
ini_lib = static_library('ini', ini_src)
mongoose_lib = static_library('mongoose', mongoose_src)


curl_dep = dependency('libcurl')
openssl_dep = dependency('openssl')
icu_dep = dependency('icu-uc')
ncurses_dep = dependency('ncursesw')
gmime_dep = dependency('gmime-2.6')


connectivity = executable(
    'connectivity',
    'connectivity/connectivity.cpp',
    'contrib/inn/date.cpp',
    include_directories: 'contrib/inn',
    link_with: [common_lib, lz4_lib],
    install: true
)

export_messages = executable(
    'export-messages',
    'export-messages/export-messages.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

extract_msgid = executable(
    'extract-msgid',
    'extract-msgid/extract-msgid.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

extract_msgmeta = executable(
    'extract-msgmeta',
    'extract-msgmeta/extract-msgmeta.cpp',
    'extract-msgmeta/tin.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

filter_newsgroups = executable(
    'filter-newsgroups',
    'filter-newsgroups/filter-newsgroups.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

filter_spam = executable(
    'filter-spam',
    'filter-spam/filter-spam.cpp',
    link_with: [common_lib, lz4_lib, terminator_lib, zstd_lib],
    install: true
)

galaxy_util = executable(
    'galaxy-util',
    'galaxy-util/galaxy-util.cpp',
    link_with: [common_lib, uat_lib, zstd_lib],
    install: true
)

google_groups = executable(
    'google-groups',
    'google-groups/google-groups.cpp',
    link_with: common_lib,
    dependencies: [curl_dep, openssl_dep],
    install: true
)

import_source_maildir = executable(
    'import-source-maildir',
    'import-source-maildir/import-source-maildir.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

import_source_maildir_7z = executable(
    'import-source-maildir-7z',
    'import-source-maildir-7z/import-source-maildir-7z.cpp',
    link_with: [common_lib, lz4_lib, lzma_lib],
    install: true
)

import_source_mbox = executable(
    'import-source-mbox',
    'import-source-mbox/import-source-mbox.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

kill_duplicates = executable(
    'kill-duplicates',
    'kill-duplicates/kill-duplicates.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

lexdist = executable(
    'lexdist',
    'lexdist/lexdist.cpp',
    link_with: common_lib,
    install: true
)

lexicon = executable(
    'lexicon',
    'lexicon/lexicon.cpp',
    link_with: [common_lib, lz4_lib],
    dependencies: icu_dep,
    install: true
)

lexsort = executable(
    'lexsort',
    'lexsort/lexsort.cpp',
    link_with: common_lib,
    install: true
)

lexstats = executable(
    'lexstats',
    'lexstats/lexstats.cpp',
    link_with: common_lib,
    install: true
)

merge_raw = executable(
    'merge-raw',
    'merge-raw/merge-raw.cpp',
    link_with: common_lib,
    install: true
)

nntp_get = executable(
    'nntp-get',
    'nntp-get/nntp-get.cpp',
    'nntp-get/Socket.cpp',
    link_with: common_lib,
    install: true
)

package = executable(
    'package',
    'package/package.cpp',
    link_with: common_lib,
    install: true
)

query = executable(
    'query',
    'query/query.cpp',
    link_with: [common_lib, zstd_lib, uat_lib],
    install: true
)

query_raw = executable(
    'query-raw',
    'query-raw/query-raw.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

relative_complement = executable(
    'relative-complement',
    'relative-complement/relative-complement.cpp',
    link_with: common_lib,
    install: true
)

repack_lz4 = executable(
    'repack-lz4',
    'repack-lz4/repack-lz4.cpp',
    link_with: [common_lib, lz4_lib, zstd_lib],
    install: true
)

repack_zstd = executable(
    'repack-zstd',
    'repack-zstd/repack-zstd.cpp',
    cpp_args: ['-DZDICT_STATIC_LINKING_ONLY', '-DZSTD_MULTITHREAD'],
    link_with: [common_lib, lz4_lib, zstd_lib],
    install: true
)

sort = executable(
    'sort',
    'sort/sort.cpp',
    link_with: [common_lib, lz4_lib],
    install: true
)

tbrowser = executable(
    'tbrowser',
    'tbrowser/BitSet.cpp',
    'tbrowser/BottomBar.cpp',
    'tbrowser/Browser.cpp',
    'tbrowser/ChartView.cpp',
    'tbrowser/GalaxyOpen.cpp',
    'tbrowser/GalaxyWarp.cpp',
    'tbrowser/HeaderBar.cpp',
    'tbrowser/Help.cpp',
    'tbrowser/MessageView.cpp',
    'tbrowser/SearchView.cpp',
    'tbrowser/tbrowser.cpp',
    'tbrowser/TextView.cpp',
    'tbrowser/ThreadTree.cpp',
    'tbrowser/ThreadView.cpp',
    'tbrowser/Utf8Print.cpp',
    'tbrowser/View.cpp',
    link_with: [common_lib, zstd_lib, uat_lib],
    dependencies: [icu_dep, ncurses_dep],
    install: true
)

threadify = executable(
    'threadify',
    'threadify/threadify.cpp',
    link_with: [common_lib, zstd_lib, uat_lib],
    dependencies: icu_dep,
    install: true
)

uat = executable(
    'uat',
    'uat/uat.cpp',
    install: true
)

update_zstd = executable(
    'update-zstd',
    'update-zstd/update-zstd.cpp',
    link_with: [common_lib, lz4_lib, zstd_lib],
    cpp_args: '-DZDICT_STATIC_LINKING_ONLY',
    install: true
)

utf8ize = executable(
    'utf8ize',
    'utf8ize/utf8ize.cpp',
    link_with: [common_lib, lz4_lib],
    dependencies: gmime_dep,
    install: true
)

verify = executable(
    'verify',
    'verify/verify.cpp',
    link_with: [common_lib, zstd_lib, uat_lib],
    install: true
)

web = executable(
    'web',
    'web/web.cpp',
    link_with: [common_lib, zstd_lib, uat_lib, ini_lib, mongoose_lib],
    dependencies: icu_dep,
    install: true
)
