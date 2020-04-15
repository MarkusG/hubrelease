[![CodeFactor](https://www.codefactor.io/repository/github/markusg/hubrelease/badge)](https://www.codefactor.io/repository/github/markusg/hubrelease)

# hubrelease

hubrelease allows you to create GitHub releases from the command line. hubrelease automatically detects a repository's upstream GitHub repository from its remotes, prompting the user for input if multiple GitHub repositories are found. Users can optionally add release assets, and edit the release messages in the terminal, similar to `git commit`.

## Basic Usage
```
$ git tag -a v1.0
$ tar -xaf mypackage.tar.gz myfiles
$ git push --follow-tags
$ hubrelease --token yourtokenhere mypackage.tar.gz
```
Token can also be passed through an environment variable:
```
$ git tag -a v1.0
$ tar -xaf mypackage.tar.gz myfiles
$ git push --follow-tags
$ export HUBRELEASE_GITHUB_TOKEN=yourtokenhere
$ hubrelease mypackage.tar.gz
```

## Dependencies
libcurl, libgit2, jansson
