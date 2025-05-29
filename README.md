## Building the Application

The application is located in the `app` folder. To build it, follow these steps:

### 1. Download and Install the Toolchain

- Toolchain link: [http://test.ai-thinker.com/csdk/CSDTK42.7z](http://test.ai-thinker.com/csdk/CSDTK42.7z)
- Decompress the archive to a folder, for example: `C:\CSDTK`
- Run the `config_env_admin.bat` file in the CSDTK directory to set the environment variables.

### 2. Get the SDK with the Application

Clone the gps traccer app repository:

```bash
git clone https://github.com/NewoPL/A9G-GPS-Tracker.git
```

### 3. Build the Application

Navigate to the project directory and run:

```bash
./build.bat app
```

After compilation, a build folder will be generated with the target files for flashing to the A9G board.

### 4.Flash to the A9G Board
Inside the build/hex folder there are two .lod files: one with _B*.lod and one with *_flash.lod.
Flash the larger file the first time or when updating the SDK version.
For subsequent uploads, you can flash the smaller file to reduce download time.
