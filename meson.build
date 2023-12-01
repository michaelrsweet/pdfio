project('pdfio', 'c',
    version: '1.1.4',
    meson_version: '>=0.49.0',
    default_options: [
        'optimization=s',
        'debug=true',
        'default_library=static',
    ]
)

cc = meson.get_compiler('c')

deps = [
    cc.find_library('m', required: false),
    dependency('zlib', version: '>=1.0'),
]


pubheaders = [
    'pdfio.h',
    'pdfio-content.h',
]
pubsrcs = [
    'pdfio-aes.c',
    'pdfio-array.c',
    'pdfio-common.c',
    'pdfio-content.c',
    'pdfio-crypto.c',
    'pdfio-dict.c',
    'pdfio-file.c',
    'pdfio-md5.c',
    'pdfio-object.c',
    'pdfio-page.c',
    'pdfio-rc4.c',
    'pdfio-sha256.c',
    'pdfio-stream.c',
    'pdfio-string.c',
    'pdfio-token.c',
    'pdfio-value.c',
]
libsrcs = pubsrcs + [
    'ttf.c',
]
srcs = pubsrcs + [
    'pdfiototext.c',
    'testpdfio.c',
    'testttf.c',
]

_libpdfio = static_library('_pdfio', libsrcs,
    dependencies: deps,
)
libpdfio = library('pdfio',
    dependencies: deps,
    link_whole: _libpdfio,
    vs_module_defs: 'pdfio1.def',
    version: '1',
    darwin_versions: ['1.0', meson.project_version()],
    install: true
)
pdfio_dep = declare_dependency(
    link_with: libpdfio,
    include_directories: include_directories('.'),
)
import('pkgconfig').generate(libpdfio,
    description: 'PDF read/write library',
    url: 'https://www.msweet.org/pdfio',
)
install_headers(pubheaders)
install_man('doc/pdfio.3')
install_data('doc/pdfio.html', 'doc/pdfio-512.png', 'LICENSE', 'NOTICE',
    install_dir: get_option('datadir') / 'doc/pdfio',
)

executable('pdfiototext', 'pdfiototext.c',
    dependencies: deps,
    link_with: libpdfio,
)


# tests
testttf = executable('testttf', 'testttf.c', 'ttf.c',
    dependencies: deps,
)
test('testttf', testttf,
    workdir: meson.current_source_dir(),
)
testpdfio = executable('testpdfio', 'testpdfio.c',
    dependencies: deps,
    # links with private APIs
    link_with: _libpdfio,
)
test('testpdfio', testpdfio,
    workdir: meson.current_source_dir(),
)

add_test_setup('valgrind',
    exe_wrapper: ['valgrind', '--leak-check=full'],
)
