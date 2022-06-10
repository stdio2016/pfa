# pfa

This is an implementation of landmark-based Audio FingerPrint.
Also the baseline model used in my thesis, "Improvement of Neural Network- and Landmark-based Audio Fingerprinting".
See <https://github.com/stdio2016/pfann> for more information.

To recognize a music, you need to build a fingerprint database, then search from this database.
In this repo, I refer to the songs as music, because audio fingerprint shouldn't be limited to songs with vocals, it should be able to search instrumental music as well.

## Requirements

C++ programs (`builder`, `matcher`, `getlm` and `finddup`):
1. OpenMP enabled C++ compiler
2. Make
3. (if you want to compile `finddup`) MPI (Message Passing Interface)

Python programs (in `tools/` and `adversary/` folder):
1. pydub
2. matplotlib
3. scipy
4. miniaudio
5. soundfile
6. numpy
7. FFMPEG

## Building
On linux, just type `make all` to compile `builder`, `matcher` and `getlm`.

On Windows, you need to compile with MinGW, or run the following commands in Visual Studio command prompt:
```
cl /EHsc /O2 /openmp builder.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp PeakFinder.cpp PeakFinderDejavu.cpp Analyzer.cpp
cl /EHsc /O2 /openmp matcher.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp .\PeakFinder.cpp .\PeakFinderDejavu.cpp Analyzer.cpp
cl /EHsc /O2 /openmp getlm.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp PeakFinder.cpp PeakFinderDejavu.cpp Analyzer.cpp
```
Note: I have only tested on Visual Studio 2015.

On Mac, you might need to type:
```sh
make CXXFLAGS='-std=c++11 -O3 -Xpreprocessor -fopenmp -I /opt/homebrew/include' LIBS=/opt/homebrew/lib/libomp.dylib all
```

To compile `finddup`, please type `make finddup`. MPI is required. Currently only tested on Linux.

## Usage

### Build a fingerprint database
```sh
./builder <music list file> <output database location>
```
Music list file is a file containing list of music file paths.
File must be UTF-8 without BOM. For example:
```
/path/to/fma_medium/000/000002.mp3
/path/to/fma_medium/000/000005.mp3
/path/to/your/music/aaa.wav
/path/to/your/music/bbb.wav
```
This program supports both MP3 and WAV audio format.
Relative paths are supported but not recommended.

### Recognize music
```sh
./matcher <query list> <database location> <result file>
```
Query list is a file containing list of query file paths. For example:
```
/path/to/queries/out2_snr2/000002.wav
/path/to/queries/out2_snr2/000005.wav
/path/to/song_recorded_on_street1.wav
/path/to/song_recorded_on_street2.wav
```
Database location is the place where `builder` saves database.

The result file will be a TSV file with 2 fields: query file path, and matched music path, but without header.
It may look like this:
```
/path/to/queries/out2_snr2/000002.wav	/path/to/fma_medium/000/000002.mp3
/path/to/queries/out2_snr2/000005.wav	/path/to/fma_medium/000/000005.mp3
/path/to/song_recorded_on_street1.wav	/path/to/your/music/aaa.wav
/path/to/song_recorded_on_street2.wav	/path/to/your/music/aaa.wav
```

Matcher will also generate a CSV file and a BIN file.
CSV file contains more information about the matches.
It has 5 columns: query, answer, score, time, and part_scores.
* query: Query file path
* answer: Matched music path
* score: Matching score, used in my thesis
* time: The time when the query clip starts in the matched music, in seconds
* part_scores: Mainly used for debugging, currently empty

BIN file contains matching scores of every database music for each query.
It is used in my ensemble experiments.
The file format is a flattened 2D array of following structure, without header:
```c++
struct match_t {
  float score; // Matching score
  float offset; // The time when the query clip starts in the matched music, in seconds
};
```
The matching score of j-th database music in i-th query is at index `i * database size + j`.

### getlm
`getlm` is a utility for landmark extraction. Usage:
```sh
./getlm <audio file> <output> [REPEAT_COUNT]
```
REPEAT_COUNT is for my experiment, "對查詢做多次時間位移," in section 4.5.3 of my thesis. 
REPEAT_COUNT is called N in that section.
(TODO: translate my thesis)

Accourding to my thesis, REPEAT_COUNT is 1 when creating database, then REPEAT_COUNT is 1, 2, or 4 when querying.

Output file will be a binary file without header, with extension `.lm`. Its format is an array of the following struct:
```C++
struct Landmark {
  int32_t time1;
  int32_t freq1;
  float energy1;
  int32_t time2;
  int32_t freq2;
  float energy2;
};
```

If REPEAT_COUNT is set to `peaks`, then it outputs array of peaks instead.
```C++
struct Peak {
  int32_t time;
  int32_t freq;
  float energy;
};
```

### finddup
`finddup` is a utility for finding possibly duplicate music.
It uses landmark-based audio fingerprint, the same technique as this repo.
It works by splitting every music to 10-second clips, then query those clips in database.
It is designed to be run on a supercomputer, so it uses both OpenMP and MPI.
Usage:
```sh
mpiexec [mpi params] ./finddup $peak_file_list $database_dir $result_file
```
You need to first preprocess music:
1. Build database using `./builder $music_list $database_dir`.
2. For each `$music` in `$music_list`, extract peaks using `./getlm $music $out_peak_file peaks`.
3. Save each `$out_peak_file` name to a file `$peak_file_list`.

The program will output to `${result_file}-pid-${pid}`, for each process `$pid`.
They are CSV files with 22 fields, without header.
Each row corresponds to a clip of music.
* Field 1: music id (0-based)
* Field 2: position of clip in that music, in seconds
* Field 2n + 1: n-th best music match
* Field 2n + 2: the time when the query clip starts in the matched music

The program only outputs top-10 matches to save space.
