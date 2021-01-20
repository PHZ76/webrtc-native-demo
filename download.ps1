$SOURCE_DIR = Join-Path (Resolve-Path ".").Path "_download"
$INSTALL_DIR = Join-Path (Resolve-Path ".").Path "dependencies"

Remove-Item $SOURCE_DIR -Force -Recurse -ErrorAction Ignore
Remove-Item $INSTALL_DIR\webrtc -Force -Recurse -ErrorAction Ignore

mkdir $SOURCE_DIR -ErrorAction Ignore
mkdir $INSTALL_DIR -ErrorAction Ignore

if (!(Test-Path "$INSTALL_DIR\webrtc\lib\x64\release\webrtc.lib")) {
  $URL = "https://github.com/PHZ76/webrtc-native-demo/releases/download/webrtc-m85/webrtc.zip"
  $FILE = "$SOURCE_DIR\webrtc.zip"
  
  Push-Location $SOURCE_DIR
    if (!(Test-Path $FILE)) {
      Invoke-WebRequest -Uri $URL -OutFile $FILE
    }
  Pop-Location

  Remove-Item $SOURCE_DIR\webrtc -Recurse -Force -ErrorAction Ignore

  Push-Location $SOURCE_DIR
    7z x $FILE
  Pop-Location

  Remove-Item $INSTALL_DIR\webrtc -Recurse -Force -ErrorAction Ignore
  Move-Item $SOURCE_DIR\webrtc $INSTALL_DIR\webrtc
}

