project(
  'hubrelease',
  'c',
  version: '0.1.1'
)

version = '-DVERSION="@0@"'
version = version.format(meson.project_version())
add_global_arguments(version, language : 'c')

deps = [
  dependency('libgit2'),
  dependency('libcurl'),
  dependency('jansson')
]

subdir('hubrelease')
