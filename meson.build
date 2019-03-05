project('mediaelch', 'cpp', 'c',
    version: '2.6.1-dev',
    default_options: ['c_std=c11', 'cpp_std=c++14', 'warning_level=3']
)

cc = meson.get_compiler('cpp')

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules : [
    'Widgets',
    'Core',
    'Gui',
    'Network',
    'Xml',
    'Sql',
    'Multimedia',
    'MultimediaWidgets',
    'Concurrent',
    'Qml',
    'Quick',
    'QuickWidgets'
])

resources = [
    'data/MediaElch.qrc',
    'data/i18n.qrc',
    'ui.qrc'
]

includes = include_directories(['src', 'third_party'])

dl_dep = cc.find_library('dl')

subdir('third_party')
subdir('src')

install_data('MediaElch.plist', install_dir: 'Contents')
