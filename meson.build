project(
  'hubrelease',
  'c',
  version: '0.1.4'
)

release_date = 'December 26, 2019'

version = '-DVERSION="@0@"'
version = version.format(meson.project_version())
add_global_arguments(version, language : 'c')

version_conf = configuration_data()

version_conf.set(
  'HUBRELEASE_VERSION',
  meson.project_version()
)

version_conf.set(
  'HUBRELEASE_RELEASE_DATE',
  release_date
)

deps = [
  dependency('libgit2'),
  dependency('libcurl'),
  dependency('jansson')
]

subdir('hubrelease')
subdir('doc')
