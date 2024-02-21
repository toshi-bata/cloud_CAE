# Cloud CAE demo
Cloud CAE demo is able to show a CAE workflow such as 3D data import, model editing, pre-process (mesh), solve and post-process using HOOPS & Ceetron SDKs.  
This demo is introduced in Tech Soft 3D YouTube channel<br>
https://www.youtube.com/watch?v=u2jEBdmtBO0

## Dependencies
### SDKs in use (version)
- HOOPS Communicator (2024)
- HOOPS Exchange (2024)
- Parasolid (34.1)
- Ceetron Envition for Web (1.8.0_b1eca36a)
- Ceetron SAM-Eval (1.8.0_22e9e1ad)
- Ceetron Envision for Desktop (1.8.0_b1eca36a)

### Third paty module
- libmicrohttpd (1.0.0-w32-bin)
  https://ftp.gnu.org/gnu/libmicrohttpd/

## Setup
### Demo folders
&emsp;+ cloud_CAE
&emsp;&emsp;+ css<br>
&emsp;&emsp;+ js<br>
&emsp;&emsp;+ PsServer<br>
&emsp;&emsp;+ Release<br>
&emsp;&emsp;&emsp;+ scheme (Locate schema folder from `Parasolid SDK\base`)<br>
&emsp;&emsp;&emsp;+ PsServer.exe<br>
&emsp;&emsp;&emsp;+ Parasolid dll files (locate all files in `Parasolid SDK\base\bin`)<br>
&emsp;&emsp;&emsp;+ Exchange dll files (locate all files in `Exchange SDK\bin\win64_v142`)<br>
&emsp;&emsp;+ src (copy src folder from `Communicator SDK\web_viewer`)<br>
&emsp;&emsp;+ CEW (copy src folder from `Communicator SDK\web_viewer`)<br>
&emsp;&emsp;&emsp;+ Common (copy Common folder from `CEW SDK\Examples`)<br>
&emsp;&emsp;+ cloud_CAE.html<br>
&emsp;&emsp;+ HttpServer.js<br>
&emsp;&emsp;+ package.json<br>

### Environment valiables
Register the following enviroment valiables
    Name: `CEW_UG_MODEL_PATH`       Value: <Envision Web SDK>\TestModels\ (Put "\" at the last)
    Name: `PS_SERVER_WORKING_DIR`   Value: C:\temp\PsServer\ (Create the folder if it doen't exist)

### PsServer
PsServer is a server side console application which is using HOOPS Exchange, Parasolid, Ceetron SAM. <br>
\* As of now, it will works on Windows only
If you want to rebuild ProcessServer or PsServer application<br>
1. Edit PsServer\HC_VS2019.bat according to your environment
2. Launch the `PsServer.sln` solution with VS2019 using the above bat file
3. Build the console application project

### Ceetron Envision for Web - UG server
1. Set a valid license key in <Envision Web SDK>\server\UgServer\Main.js
2. Launch the UG server using a command prompt<br>
    `cd …\server\UgServer`<br>
    `npm install`<br>
    `npm run startWin`<br>

### Ceetron Envision for Desktop
1. Overwrite .cpp, .h and .ui files in the QtDemoApp folder to <Envision Desktop SDK>\Examples\Qt\QtDemoApp
2. Open the project using QT Creator and Build 

## Start cloud_CAE for debug (without using ProcessServer)
1. Launch HTTP server using a command prompt<br>
    `cd …/cloud_CAE`
    `npm install`
    `npm start`
2. Start PsServer.exe 8889 (giving a port number in command line argument)
3. Open (using Chrome)<br>
    http://localhost:8000/cloud_CAE.html?viewer=scs&instance=_empty.scs&debug=on&port=8889

