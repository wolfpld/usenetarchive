cmake_minimum_required(VERSION 3.29)

option(TRACY_ENABLE "Enable Tracy" OFF)
option(MARCH_NATIVE "Enable -march=native" ON)

set(CMAKE_CXX_STANDARD 17)

project(UsenetArchive
    LANGUAGES ASM C CXX
)

include(CheckCompilerFlag)
include(FindPkgConfig)
include(cmake/config.cmake)
include(cmake/CPM.cmake)

if(NOT TRACY_ENABLE)
    set(TRACY_OPTIONS "TRACY_STATIC ON")
endif()

CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG master
    OPTIONS ${TRACY_OPTIONS}
    EXCLUDE_FROM_ALL TRUE
)

if(TRACY_ENABLE)
    add_definitions(-DTRACY_ENABLE)
    add_compile_options(-g -O3 -fno-omit-frame-pointer)
endif()

if(MARCH_NATIVE)
    check_compiler_flag(CXX "-march=native" HAS_MARCH_NATIVE)
    if(HAS_MARCH_NATIVE)
        add_compile_options(-march=native)
    endif()
endif()


pkg_check_modules(CURL REQUIRED libcurl)
add_library(curl INTERFACE)
target_link_libraries(curl INTERFACE ${CURL_LINK_LIBRARIES})
target_include_directories(curl INTERFACE ${CURL_INCLUDE_DIRS})

pkg_check_modules(OPENSSL REQUIRED openssl)
add_library(ssl INTERFACE)
target_link_libraries(ssl INTERFACE ${OPENSSL_LINK_LIBRARIES})
target_include_directories(ssl INTERFACE ${OPENSSL_INCLUDE_DIRS})

pkg_check_modules(ICU REQUIRED icu-uc)
add_library(icu INTERFACE)
target_link_libraries(icu INTERFACE ${ICU_LINK_LIBRARIES})
target_include_directories(icu INTERFACE ${ICU_INCLUDE_DIRS})

pkg_check_modules(NCURSES REQUIRED ncursesw)
add_library(ncurses INTERFACE)
target_link_libraries(ncurses INTERFACE ${NCURSES_LINK_LIBRARIES})
target_include_directories(ncurses INTERFACE ${NCURSES_INCLUDE_DIRS})

pkg_check_modules(GMIME REQUIRED gmime-3.0)
add_library(gmime INTERFACE)
target_link_libraries(gmime INTERFACE ${GMIME_LINK_LIBRARIES})
target_include_directories(gmime INTERFACE ${GMIME_INCLUDE_DIRS})

pkg_check_modules(LZ4 REQUIRED liblz4)
add_library(lz4 INTERFACE)
target_link_libraries(lz4 INTERFACE ${LZ4_LINK_LIBRARIES})
target_include_directories(lz4 INTERFACE ${LZ4_INCLUDE_DIRS})


set(COMMON_SRC
    common/Filesystem.cpp
    common/ICU.cpp
    common/KillRe.cpp
    common/LexiconTypes.cpp
    common/MessageLines.cpp
    common/MessageLogic.cpp
    common/mmap.cpp
    common/ParseDate.cpp
    common/StringCompress.cpp
    common/System.cpp
    common/TaskDispatch.cpp
    common/UTF8.cpp
    contrib/xxhash/xxhash.c
)

set(LIBUAT_SRC
    libuat/Archive.cpp
    libuat/Galaxy.cpp
    libuat/PackageAccess.cpp
    libuat/PersistentStorage.cpp
    libuat/SearchEngine.cpp
)

set(ZSTD_SRC
    contrib/zstd/common/debug.c
    contrib/zstd/common/entropy_common.c
    contrib/zstd/common/error_private.c
    contrib/zstd/common/fse_decompress.c
    contrib/zstd/common/pool.c
    contrib/zstd/common/threading.c
    contrib/zstd/common/xxhash.c
    contrib/zstd/common/zstd_common.c
    contrib/zstd/compress/fse_compress.c
    contrib/zstd/compress/hist.c
    contrib/zstd/compress/huf_compress.c
    contrib/zstd/compress/zstd_compress_literals.c
    contrib/zstd/compress/zstd_compress_sequences.c
    contrib/zstd/compress/zstd_compress_superblock.c
    contrib/zstd/compress/zstd_compress.c
    contrib/zstd/compress/zstd_double_fast.c
    contrib/zstd/compress/zstd_fast.c
    contrib/zstd/compress/zstd_lazy.c
    contrib/zstd/compress/zstd_ldm.c
    contrib/zstd/compress/zstd_opt.c
    contrib/zstd/compress/zstdmt_compress.c
    contrib/zstd/decompress/huf_decompress.c
    contrib/zstd/decompress/huf_decompress_amd64.S
    contrib/zstd/decompress/zstd_ddict.c
    contrib/zstd/decompress/zstd_decompress_block.c
    contrib/zstd/decompress/zstd_decompress.c
    contrib/zstd/dictBuilder/cover.c
    contrib/zstd/dictBuilder/divsufsort.c
    contrib/zstd/dictBuilder/fastcover.c
    contrib/zstd/dictBuilder/zdict.c
)

set(TERMINATOR_SRC
    contrib/kyotocabinet/kccachedb.cc
    contrib/kyotocabinet/kccompare.cc
    contrib/kyotocabinet/kccompress.cc
    contrib/kyotocabinet/kcdb.cc
    contrib/kyotocabinet/kcdbext.cc
    contrib/kyotocabinet/kcdirdb.cc
    contrib/kyotocabinet/kcfile.cc
    contrib/kyotocabinet/kchashdb.cc
    contrib/kyotocabinet/kclangc.cc
    contrib/kyotocabinet/kcmap.cc
    contrib/kyotocabinet/kcplantdb.cc
    contrib/kyotocabinet/kcpolydb.cc
    contrib/kyotocabinet/kcprotodb.cc
    contrib/kyotocabinet/kcregex.cc
    contrib/kyotocabinet/kcstashdb.cc
    contrib/kyotocabinet/kctextdb.cc
    contrib/kyotocabinet/kcthread.cc
    contrib/kyotocabinet/kcutil.cc
    contrib/terminator/terminator_classifier_base.cc
    contrib/terminator/terminator_classifier_bwinnow.cc
    contrib/terminator/terminator_classifier_hit.cc
    contrib/terminator/terminator_classifier_lr.cc
    contrib/terminator/terminator_classifier_nb.cc
    contrib/terminator/terminator_classifier_nsnb.cc
    contrib/terminator/terminator_classifier_owv.cc
    contrib/terminator/terminator_classifier_pa.cc
    contrib/terminator/terminator_classifier_pam.cc
    contrib/terminator/terminator_classifier_winnow.cc
    contrib/terminator/terminator.cc
)

set(LZMA_SRC
    contrib/lzma/7zBuf.c
    contrib/lzma/7zBuf2.c
    contrib/lzma/7zCrc.c
    contrib/lzma/7zCrcOpt.c
    contrib/lzma/7zDec.c
    contrib/lzma/7zFile.c
    contrib/lzma/7zIn.c
    contrib/lzma/7zStream.c
    contrib/lzma/Bcj2.c
    contrib/lzma/Bra.c
    contrib/lzma/Bra86.c
    contrib/lzma/CpuArch.c
    contrib/lzma/Lzma2Dec.c
    contrib/lzma/LzmaDec.c
    contrib/lzma/Ppmd7.c
    contrib/lzma/Ppmd7Dec.c
)

set(INI_SRC
    contrib/ini/ini.c
)

set(MONGOOSE_SRC
    contrib/mongoose/mongoose.c
)

set(INN_SRC
    contrib/inn/date.cpp
)

add_library(common ${COMMON_SRC})
add_library(libuat ${LIBUAT_SRC})
add_library(terminator STATIC ${TERMINATOR_SRC})
target_include_directories(terminator PUBLIC ${CMAKE_SOURCE_DIR}/contrib/kyotocabinet)
add_library(lzma STATIC ${LZMA_SRC})
add_library(ini STATIC ${INI_SRC})
add_library(mongoose STATIC ${MONGOOSE_SRC})
add_library(inn ${INN_SRC})
target_include_directories(inn PRIVATE ${CMAKE_SOURCE_DIR}/contrib/inn)
add_library(zstd STATIC ${ZSTD_SRC})
target_compile_definitions(zstd PUBLIC ZDICT_STATIC_LINKING_ONLY ZSTD_MULTITHREAD)

add_executable(connectivity connectivity/connectivity.cpp)
target_link_libraries(connectivity PRIVATE common inn lz4)

add_executable(export-messages export-messages/export-messages.cpp)
target_link_libraries(export-messages PRIVATE common lz4)

add_executable(extract-msgid extract-msgid/extract-msgid.cpp)
target_link_libraries(extract-msgid PRIVATE common lz4)

add_executable(extract-msgmeta extract-msgmeta/extract-msgmeta.cpp extract-msgmeta/tin.cpp)
target_link_libraries(extract-msgmeta PRIVATE common lz4)

add_executable(filter-newsgroups filter-newsgroups/filter-newsgroups.cpp)
target_link_libraries(filter-newsgroups PRIVATE common lz4)

add_executable(filter-spam filter-spam/filter-spam.cpp)
target_link_libraries(filter-spam PRIVATE terminator common zstd lz4)

add_executable(galaxy-util galaxy-util/galaxy-util.cpp)
target_link_libraries(galaxy-util PRIVATE common zstd libuat)

add_executable(google-groups google-groups/google-groups.cpp)
target_link_libraries(google-groups PRIVATE curl common ssl)

add_executable(import-source-maildir import-source-maildir/import-source-maildir.cpp)
target_link_libraries(import-source-maildir PRIVATE common lz4)

add_executable(import-source-maildir-7z import-source-maildir-7z/import-source-maildir-7z.cpp)
target_link_libraries(import-source-maildir-7z PRIVATE common lz4 lzma)

add_executable(import-source-mbox import-source-mbox/import-source-mbox.cpp)
target_link_libraries(import-source-mbox PRIVATE common lz4)

add_executable(kill-duplicates kill-duplicates/kill-duplicates.cpp)
target_link_libraries(kill-duplicates PRIVATE common lz4)

add_executable(lexdist lexdist/lexdist.cpp)
target_link_libraries(lexdist PRIVATE common)

add_executable(lexicon lexicon/lexicon.cpp)
target_link_libraries(lexicon PRIVATE common icu lz4)

add_executable(lexsort lexsort/lexsort.cpp)
target_link_libraries(lexsort PRIVATE common)

add_executable(lexstats lexstats/lexstats.cpp)
target_link_libraries(lexstats PRIVATE common)

add_executable(merge-raw merge-raw/merge-raw.cpp)
target_link_libraries(merge-raw PRIVATE common)

add_executable(nntp-get nntp-get/nntp-get.cpp nntp-get/Socket.cpp)
target_link_libraries(nntp-get PRIVATE common inn)

add_executable(package package/package.cpp)
target_link_libraries(package PRIVATE common)

add_executable(query query/query.cpp)
target_link_libraries(query PRIVATE common zstd libuat)

add_executable(query-raw query-raw/query-raw.cpp)
target_link_libraries(query-raw PRIVATE common lz4)

add_executable(relative-complement relative-complement/relative-complement.cpp)
target_link_libraries(relative-complement PRIVATE common)

add_executable(repack-lz4 repack-lz4/repack-lz4.cpp)
target_link_libraries(repack-lz4 PRIVATE common zstd lz4)

add_executable(repack-zstd repack-zstd/repack-zstd.cpp)
target_link_libraries(repack-zstd PRIVATE zstd common lz4)

add_executable(sort sort/sort.cpp)
target_link_libraries(sort PRIVATE common)

add_executable(tbrowser
    tbrowser/BitSet.cpp
    tbrowser/BottomBar.cpp
    tbrowser/Browser.cpp
    tbrowser/ChartView.cpp
    tbrowser/GalaxyOpen.cpp
    tbrowser/GalaxyWarp.cpp
    tbrowser/HeaderBar.cpp
    tbrowser/Help.cpp
    tbrowser/MessageView.cpp
    tbrowser/SearchView.cpp
    tbrowser/tbrowser.cpp
    tbrowser/TextView.cpp
    tbrowser/ThreadTree.cpp
    tbrowser/ThreadView.cpp
    tbrowser/Utf8Print.cpp
    tbrowser/View.cpp
)
target_link_libraries(tbrowser PRIVATE ncurses libuat common icu zstd)

add_executable(threadify threadify/threadify.cpp)
target_link_libraries(threadify PRIVATE common icu zstd libuat)

add_executable(uat uat/uat.cpp)

add_executable(update-zstd update-zstd/update-zstd.cpp)
target_link_libraries(update-zstd PRIVATE zstd common lz4)

add_executable(utf8ize utf8ize/utf8ize.cpp)
target_link_libraries(utf8ize PRIVATE gmime lz4 common)

add_executable(verify verify/verify.cpp)
target_link_libraries(verify PRIVATE common zstd libuat)

add_executable(web web/web.cpp)
target_link_libraries(web PRIVATE mongoose ini common libuat zstd)
