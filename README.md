# hubrelease

hubrelease automates the creation of GitHub releases and the uploading of files to releases. hubrelease automatically detects a repository's upstream GitHub repository from its remotes, prompting the user for input if multiple GitHub repositories are found. Users can optionally edit the release messages in the terminal, similar to `git commit`.

## Basic Usage
```
$ git tag -a v1.0
$ tar -xaf mypackage.tar.gz myfiles
$ hubrelease --asset mypackage.tar.gz
```

## Options
```
--draft:         create a draft release
--prerelease:    create a prerelease
--remote REMOTE: specify the remote to use
--asset FILE:    specify file to upload (can be used multiple times)
```

## Dependencies
libcurl, libgit2, jansson
