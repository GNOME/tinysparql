if (get_option('webpack'))

    webpack = custom_target(
        'webpack',
        output: ['main.js', 'index.html'],
        command: ['./webpack.sh']
    )

    deps = [webpack]
else
    web_ide = files(
        'dist/main.js',
        'dist/index.html'
    )
    deps = [web_ide]
endif

web_ide_gresources = gnome.compile_resources(
    'web_ide_gresources', 
    'web-ide.gresource.xml',
    dependencies: deps
)

