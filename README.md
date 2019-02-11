# Updater and Launcher 

Automatic updater & launcher

[日本語](/README.ja.md)

## Description 

so-called automatic updater / launcher.

Download the latest image file (ZIP file) from the server, reflect it locally, and kick the application.

A special mechanism is unnecessary on the server side. Any server can be used as long as static file delivery is possible.

## Requirement 

works in the windows environment. There is nothing especially necessary.

The following libraries are required for the build. We prepare bat to build below external\\
- boost
- zlib
- openssl
- yaml-cpp

## Usage

Write the setting in Usage UpdateLauncher.yml and put it in the same location as UpdateLauncher.exe.

After that, simply execute UpdateLauncher.exe, update it as necessary,
and kick the application.

The updater is a zip file. Compress the latest version of the file with a general zip tool and place it on the server. Since only files necessary for updating are downloaded, it is possible to include all the files in zip.

### UpdateLauncher.yml (example)

```
# directory to expand zip 
# - "$(ExeDir)" -> will be replaced with directory with UpdateLauncehr.exe 
targetDir: $(ExeDir) 

# Download URL
# - Only files that do not exist locally or differ are downloaded. 
remoteZip: https://user:12345678@example.com/software.zip 

# Finally kick EXE 
# - "$(ExeDir)" -> Replaced with directory with UpdateLauncehr.exe 
launchExe: $(ExeDir)boot.exe 
```

The application log is output to the folder called

## Feature and Limitations 

- The application log is output to the folder called "UpdateLauncher.log\\"

- Update yourself (UpdateLauncher.exe) is also possible. You can include your own updater in the remote ZIP.
  
- Not supported for zip64 and encrypted zip. It is not compatible with compression format other than deflate.

- Since it is a command line tool, it can be used only by kicking.

- No special mechanism is required on the server side. Any server can be used as long as static file delivery is possible.

- Only files necessary for updating are downloaded. It is not to download the entire zip file.

- Since data in a compressed state flows on the communication path, it does not waste bandwidth.

- Digest authentication is not supported. When access restriction is necessary, HTTPS + basic authentication is recommended.

- It is not supported for operation under proxy environment.

- Some functions have windows dependency code.

## Licence

[LICENSE](/LICENSE)
