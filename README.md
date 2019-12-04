# hubrelease

hubrelease automates the creation of GitHub releases and the uploading of files to releases. hubrelease automatically detects a repository's upstream GitHub repository from its remotes, prompting the user for input if multiple GitHub repositories are found. Users can optionally edit the release messages in the terminal, similar to `git commit`.

## Basic Usage
```
$ git tag -a v1.0
$ tar -xaf mypackage.tar.gz myfiles
$ hubrelease --asset mypackage.tar.gz --token yourtokenhere
```
Token can also be passed through an environment variable:
```
$ git tag -a v1.0
$ tar -xaf mypackage.tar.gz myfiles
$ export HUBRELEASE_GITHUB_TOKEN=yourtokenhere
$ hubrelease --asset mypackage.tar.gz
```

## Options
```
--draft            create a draft release
--prerelease       create a prerelease
--token TOKEN      specify GitHub token
--generate-token   generate GitHub token
--remote REMOTE    specify the remote to use
--assets FILE ...  specify files to upload
```

## Dependencies
libcurl, libgit2, jansson
