# Real-Time-Convolver
A real-time, multithreaded BRIR (binaural room impulse response) convolver, made for use with VR microphones. Currently only supports the Oculus Rift on Windows. This algorithm was adapted from the one described in William Gardner's 1995 paper, "Efficient Convolution without Input/Output Delay."

Use:

Build on Visual Studio by first including the libs folder as a header directory. The build also requires linking with PortAudio (http://www.portaudio.com/) for compatibility with the demo option for using your computer's default speaker and microphone instead of an Oculus Rift. The .dll and .lib files are provided along with the license information. This program only supports x64 machines.

BRIR file loading is currently hard coded. To change, replace the provided "brirs\\empty_apartment_bedroom_06.wav" in the LoadBRIR::load() function with the path to the BRIR wav file. 

WASAPIInterface.cpp currently contains the logic for handling audio and interfacing with the convolution engine, but the engine is entirely indepdent of this implementation. The program builds as a command line interface which automatically detects the Oculus Rift and loads the BRIR file.

NOTE: Because of low latency constraints, the default Oculus Rift audio driver must be replaced in the device manager with the generic Windows USB audio driver.

Press 's' and then enter to start convolution with an Oculus Rift if you have one installed, otherwise press 'o' and then enter to use your computer's default speaker and microphone. If doing the latter, make sure to use headphones to prevent feedback. This option will be higher latency due to using PortAudio instead of the low level WASAPI, and may contian more audio glitches for now.

Known Issues:

- There may be volume normalization issues between different BRIR files
- BRIR files that are not 44100Hz are not normalized for playback speed yet
- Buffer underruns may cause instability, especially if the BRIR file is too long
