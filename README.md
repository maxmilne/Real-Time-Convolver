# Real-Time-Convolver
A real-time, multithreaded BRIR (binaural room impulse response) convolver, made for use with VR microphones. Currently only supports the Oculus Rift on Windows. 

Use:

Build on Visual Studio by simply including the libs folder as a header directory. BRIR file loading is currently hard coded. To change, replace "brir.wav" in the LoadBRIR::load() function with the path to the BRIR wav file. WASAPIInterface.cpp currently contains the logic for handling audio and interfacing with the convolution engine, but the engine is entirely indepdent of this implementation. The program builds as a command line interface which automatically detects the Oculus Rift and loads the BRIR file.

NOTE: Because of low latency constraints, the default Oculus Rift audio driver must be replaced in the device manager with the generic Windows USB audio driver.

Press 's' to start and 'q' to quit the convolution demo.

Known Issues:

- There may be volume normalization issues between different BRIR files
- BRIR files that are not 44100Hz are not normalized for playback speed yet
- Buffer underruns may cause instability, especially if the BRIR file is too long
