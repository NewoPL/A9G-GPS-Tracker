The application is in the app folder, in order to build it

- Download and install toolchain:
  toolchain link: http://test.ai-thinker.com/csdk/CSDTK42.7z
  Decompress the archive to a folder, for example C:\CSDTK
  Run config_env_admin.bat file in CSDTK to set environment value
 
- Get SDK with the app by cloning directly:
  git clone https://github.com/NewoPL/A9G-GPS-Tracker.git
  Type ./build.bat app or build.bat app to build app project.
  A build folder will be generated after compile, there's two *.lod files in th hex folder, it's the target file that flash to A9G board.
  There's two hex file,(*_B*.lod and *_flash.lod), you must burn the bigger one to dev board at the first time,
  then you can just burn the little one to reduce the doanload time. And you must download the bigger if you update the SDK version
