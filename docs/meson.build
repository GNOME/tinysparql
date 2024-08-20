uninstalled_manpath = meson.current_build_dir()

man_devenv = environment()
man_devenv.set('MANPATH', uninstalled_manpath)
meson.add_devenv(man_devenv)

if get_option('man')
  subdir('man1')
endif
if get_option('docs')
  subdir('tools')
  subdir('reference')
endif
