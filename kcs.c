#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "bpf.h"

//#define KCS_DEBUG
//#define KCS_DEBUG_CYCLES

typedef struct FmtChunk {
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} FmtChunk;

struct ByteBuffer {
    size_t size;
    uint8_t *data;
} typedef ByteBuffer;

struct DataChunk {
    uint32_t size;
    size_t allocSize;
    uint8_t *data;
} typedef DataChunk;

typedef struct WavFile {
    uint32_t chunkSize;
    FmtChunk fmt;
    DataChunk data;
} WavFile;

size_t fbytesleft(FILE *fp) {
    size_t cur = ftell(fp);
    fseek(fp, 0L, SEEK_END);
    size_t end = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return end - cur;
}

void wavFree(WavFile *wavFile) {
    if (wavFile != NULL) {
        if (wavFile->data.data != NULL) {
            free(wavFile->data.data);
        }
        free(wavFile);
    }
}

WavFile* wavNewDefault() {
    WavFile *wavFile = malloc(sizeof(WavFile));
    wavFile->fmt.audioFormat = 1; // PCM
    wavFile->fmt.numChannels = 1; // Mono
    wavFile->fmt.sampleRate = 22050;
    wavFile->fmt.bitsPerSample = 8; // 8-bit
    wavFile->fmt.byteRate = wavFile->fmt.sampleRate * wavFile->fmt.bitsPerSample / 8;
    wavFile->fmt.blockAlign = wavFile->fmt.numChannels * wavFile->fmt.bitsPerSample / 8;

    wavFile->data.size = 0;
    wavFile->data.allocSize = 0;
    wavFile->data.data = malloc(0);
    return wavFile;
}

void wavAllocateData(WavFile *wavFile, size_t size) {
    wavFile->data.data = realloc(wavFile->data.data, size);
    wavFile->data.allocSize = size;
}

#define READ_VAL(v) nread = fread(&v, sizeof(v), 1, file)

WavFile *wavLoadFile(FILE *file) {
    WavFile *wavFile = malloc(sizeof(WavFile));

    char chunkID[4];
    size_t nread;

    // header info
    READ_VAL(chunkID);
    if (chunkID[0] != 'R' || chunkID[1] != 'I' || chunkID[2] != 'F' || chunkID[3] != 'F') {
        printf("WAV Read Error: Expected chunk ID 'RIFF' but read '%.4s' instead.\n", chunkID);
        wavFree(wavFile);
        return NULL;
    }
    READ_VAL(wavFile->chunkSize);
    READ_VAL(chunkID);
    if (chunkID[0] != 'W' || chunkID[1] != 'A' || chunkID[2] != 'V' || chunkID[3] != 'E') {
        printf("WAV Read Error: Expected chunk ID 'WAVE' but read '%.4s' instead.\n", chunkID);
        wavFree(wavFile);
        return NULL;
    }

    // fmt chunk
    READ_VAL(chunkID);
    if (chunkID[0] != 'f' || chunkID[1] != 'm' || chunkID[2] != 't' || chunkID[3] != ' ') {
        printf("WAV Read Error: Expected chunk ID 'fmt ' but read '%.4s' instead.\n", chunkID);
        wavFree(wavFile);
        return NULL;
    }
    READ_VAL(wavFile->fmt.chunkSize);
    READ_VAL(wavFile->fmt.audioFormat);
    //printf("wav audio format: %d\n", wavFile->fmt.audioFormat);
    READ_VAL(wavFile->fmt.numChannels);
    READ_VAL(wavFile->fmt.sampleRate);
    READ_VAL(wavFile->fmt.byteRate);
    READ_VAL(wavFile->fmt.blockAlign);
    READ_VAL(wavFile->fmt.bitsPerSample);


    bool hadDataChunk = false;
    // data chunk
    while (fbytesleft(file)) {
        READ_VAL(chunkID);
        //printf("Chunk: %.4s\n", chunkID);
        uint32_t size = 0;
        READ_VAL(size);
        //printf("Size: %u\n", size);
        size_t bytes_left = fbytesleft(file);
        if (chunkID[0] != 'd' || chunkID[1] != 'a' || chunkID[2] != 't' || chunkID[3] != 'a') {
            if (size > bytes_left) {
                printf("WAV Read Error: File says there are %d bytes left but the file only has %lld bytes left.\n", size, bytes_left);
                wavFree(wavFile);
                return NULL;
            }
            else {
                printf("Skipping sub-chunk '%.4s'\n", chunkID);
                fseek(file, size, SEEK_CUR);
            }
            continue;
        }
        wavFile->data.size = size;
        wavFile->data.allocSize = (size_t)size;
        wavFile->data.data = NULL;

        if (wavFile->data.allocSize > bytes_left) {
            printf("WAV Read Error: File says there are %lld bytes left but the file only has %lld bytes left.\n", wavFile->data.allocSize, bytes_left);
            wavFree(wavFile);
            return NULL;
        }

        wavFile->data.data = malloc(wavFile->data.allocSize); // TODO: sanity checks
        nread = fread(wavFile->data.data, sizeof(uint8_t), wavFile->data.allocSize, file);
        hadDataChunk = true;
        break;
    }

    if (!hadDataChunk) {
        printf("WAV Read Error: Missing data sub-chunk\n");
        wavFree(wavFile);
        return NULL;
    }
    
    return wavFile;
}

#undef READ_VAL
#define WRITE_VAL(v) nwrite = fwrite(&v, sizeof(v), 1, file)

bool wavWriteFile(WavFile *wavFile, FILE* file) {
    size_t nwrite;
    
    // header info
    char riff[4] = {'R', 'I', 'F', 'F'};
    WRITE_VAL(riff);
    wavFile->chunkSize = 4 + (8 + wavFile->fmt.chunkSize) + (8 + wavFile->data.allocSize);
    WRITE_VAL(wavFile->chunkSize);
    char wave[4] = {'W', 'A', 'V', 'E'};
    WRITE_VAL(wave);

    char fmt[4] = {'f', 'm', 't', ' '};
    WRITE_VAL(fmt);

    wavFile->fmt.chunkSize = 16; // should this really be hardcoded?
    WRITE_VAL(wavFile->fmt.chunkSize);
    WRITE_VAL(wavFile->fmt.audioFormat);
    WRITE_VAL(wavFile->fmt.numChannels);
    WRITE_VAL(wavFile->fmt.sampleRate);
    WRITE_VAL(wavFile->fmt.byteRate);
    WRITE_VAL(wavFile->fmt.blockAlign);
    WRITE_VAL(wavFile->fmt.bitsPerSample);

    // data sub-chunk
    char data[4] = {'d', 'a', 't', 'a'};
    WRITE_VAL(data);
    uint32_t size = wavFile->data.size;
    WRITE_VAL(size);
    nwrite = fwrite(wavFile->data.data, 1, size, file);

    return true;
}

#undef WRITE_VAL

uint64_t wavGetNumFrames(WavFile *wavFile) {
    // TODO: better solution, maybe like a FILE* ?
    return wavFile->data.size / (wavFile->fmt.bitsPerSample / 8);
}

bool wavIndexValid(WavFile *wavFile, size_t index) {
    return index * wavFile->fmt.numChannels * wavFile->fmt.bitsPerSample / 8 < wavFile->data.size;
}

uint8_t wavGetFrame(WavFile *wavFile, size_t index) {
    // BUG: no support for multi-byte resolutions / multiple channels
    if (!wavIndexValid(wavFile, index)) {
        printf("WAV ERR: out of bounds\n");
        return 127;
    }
    index *= wavFile->fmt.numChannels;
    uint16_t bytesPerSample = wavFile->fmt.bitsPerSample / 8;
    if (index < wavFile->data.size) {
        switch (wavFile->fmt.bitsPerSample) {
            case 8: // unsigned byte
                return wavFile->data.data[index];
            
            case 16:; // two's compliment, little endian
                uint8_t ms_byte = wavFile->data.data[index * bytesPerSample + 1];
                uint8_t ls_byte = wavFile->data.data[index * bytesPerSample];
                //return ms_byte;
                int32_t combined = (ms_byte << 8) + ls_byte;
                if (combined > 0x7FFF) {
                    combined = combined - 0xFFFF - 1;
                }
                //printf("F %d\n", combined);
                //return (2 * (float)combined / (float)0xFFFF) - 1.0;
                return (combined / 512) + 128;
            
            case 32: // float
                // TODO
                break;
            
            default:
                break;
        }
        return wavFile->data.data[index];
    }
    printf("Error: Index exceeded end of Wav File data.\n");
    return 0;
}

void wavSetFrame(WavFile *wavFile, size_t index, uint8_t value) {
    uint32_t bytesPerSample = wavFile->fmt.bitsPerSample / 8;
    size_t furthest_index = index * bytesPerSample + (bytesPerSample - 1);
    if (index >= wavFile->data.size / bytesPerSample) {
        wavFile->data.size = (index + 1) * bytesPerSample;
    }
    if (furthest_index >= wavFile->data.allocSize) {
        wavFile->data.allocSize += 1024;
        wavFile->data.data = realloc(wavFile->data.data, wavFile->data.allocSize);
        for (size_t i = wavFile->data.allocSize - 1024; i < wavFile->data.allocSize; i++) {
            wavFile->data.data[i] = 0;
        }
    }
    if (bytesPerSample == 2) {
        //uint16_t v = *(uint16_t*)(wavFile->dataChunks[0].data + (index * 2));
        wavFile->data.data[index * bytesPerSample] = 0;
        wavFile->data.data[index * bytesPerSample + 1] = value; // most significant byte?
        return;
    }
    wavFile->data.data[index] = value;
}

enum {
    PARITY_NONE,
    PARITY_ODD,
    PARITY_EVEN
} typedef ParityMode;

uint8_t cycleLength = 18;

uint8_t count_bits(uint8_t num) {
    return ((num >> 0) & 1)
         + ((num >> 1) & 1)
         + ((num >> 2) & 1)
         + ((num >> 3) & 1)
         + ((num >> 4) & 1)
         + ((num >> 5) & 1)
         + ((num >> 6) & 1)
         + ((num >> 7) & 1);
}




/* BEGIN KCS IMPL */



const uint32_t BASE_FREQ = 2400; // represents a 1
const double DIFFERENCE_EPSILON = 0.05; // TODO: decide best value for this

int KCS_sample_evaluate(WavFile *wavFile, uint64_t *frameIndex, biquad *bpf_high, biquad *bpf_low) {
    // FIXME: getFrame should return a double probably
    int32_t isample = (int32_t)wavGetFrame(wavFile, *frameIndex);
    double sample = ((2 * isample) / 255.0) - 1.0;
    //if (sample != 0) printf("%llu %f\n", *frameIndex, sample);
    double high = BiQuad(sample, bpf_high);
    double low = BiQuad(sample, bpf_low);
    if (fabs(high - low) < DIFFERENCE_EPSILON) {
        return 0;
    }
    return high >= low ? 1 : -1;
}

typedef struct half_cycle {
    int sign;
    int length;
} half_cycle;

half_cycle KCS_half_cycle(WavFile *wavFile, uint64_t *frameIndex, biquad *bpf_high, biquad *bpf_low) {
    int length = 0;
    int val = 0;
    while (val == 0 && wavIndexValid(wavFile, *frameIndex)) {
        val = KCS_sample_evaluate(wavFile, frameIndex, bpf_high, bpf_low);
        (*frameIndex)++;
        length++;
    }

    while (wavIndexValid(wavFile, *frameIndex)) {
        int v = KCS_sample_evaluate(wavFile, frameIndex, bpf_high, bpf_low);
        // Swallow 0
        if (v != val) break;
        (*frameIndex)++;
        length++;
    }
    //printf("BPF Cycle: %d len %d\n", val, length);
    half_cycle c;
    c.sign =  val;
    c.length = length;
    return c;
}

half_cycle KCS_read_leader(WavFile *wavFile, uint64_t *frameIndex, biquad *bpf_high, biquad *bpf_low) {
    double short_half = 0.5 * wavFile->fmt.sampleRate / 2400.0;
    uint64_t total_length = 0;
    uint64_t count = 0;
    while (wavIndexValid(wavFile, *frameIndex)) {
        half_cycle hc = KCS_half_cycle(wavFile, frameIndex, bpf_high, bpf_low);
        double old_avg = (double)total_length / (double)count;
        
        // include threshold (play with values a bit)
        if (count > 100 && hc.sign == 1 && hc.length > short_half * 1.5) {
            printf("Long HC %d @ %llu, len %d\n", hc.sign, *frameIndex, hc.length);
            return hc;
        }
        
        total_length += hc.length;
        count++;
        //printf("Average: %d %f\n", count, (double)total_length / (double)count);
        
    }
}

bool get_bit(WavFile *wavFile, uint8_t cyclesPerBit, uint64_t *frameIndex, biquad* bpf_high, biquad *bpf_low) {
    uint32_t frame_window = (uint32_t)round((((double)wavFile->fmt.sampleRate)*cyclesPerBit*2)/(double)BASE_FREQ);

    double short_cycle = wavFile->fmt.sampleRate / 2400.0;
    double long_cycle = wavFile->fmt.sampleRate / 1200.0;

    int count = 0;
    int length = 0;
    for (uint8_t i = 0; i < cyclesPerBit * 2; i++) {
        half_cycle hc = KCS_half_cycle(wavFile, frameIndex, bpf_high, bpf_low);
        count++;
        length += hc.length;
        //printf("hc %d %d\n", hc.sign, hc.length);
    }
    //printf("%d\n", length);
    if (length > 0.75 * long_cycle * cyclesPerBit) {
        return false;
    }

    // finish rest because short cycles
    for (uint8_t i = 0; i < cyclesPerBit * 2; i++) {
        half_cycle hc = KCS_half_cycle(wavFile, frameIndex, bpf_high, bpf_low);
        count++;
        length += hc.length;
    }

    if (length > 0.75 * short_cycle * cyclesPerBit * 2) {
        return true;
    }

    //printf("INVALID");
    return true;
}

struct {
    uint64_t parity_errors;
    ByteBuffer data;
} typedef DecodedKCS;

DecodedKCS KCS_decode(
    WavFile *wavFile, uint32_t baud, uint8_t data_bits, uint8_t stop_bits,
    uint8_t start_bits, ParityMode parity_mode, uint16_t leader
) {
    uint16_t cycles_per_bit = 1200 / baud;
    uint64_t parity_errors = 0;

    uint64_t byteIndex = 0;
    
    ByteBuffer buffer;
    buffer.size = 1024;
    buffer.data = malloc(1024);

    uint8_t bits_done;
    uint8_t num;

    uint64_t frames = wavGetNumFrames(wavFile) / wavFile->fmt.numChannels;
    uint64_t frameIndex = 0;

    bool complete = false;

    // TODO: don't hard code frequencies here
    biquad *bpf_high = BiQuad_new_BPF(2400, wavFile->fmt.sampleRate, 1.0);
    biquad *bpf_low = BiQuad_new_BPF(1200, wavFile->fmt.sampleRate, 1.0);

    while (KCS_sample_evaluate(wavFile, &frameIndex, bpf_high, bpf_low) == 0) frameIndex++;
    //frameIndex--;
    printf("Leader begin @ %d\n", frameIndex);
    half_cycle first_half_cycle = KCS_read_leader(wavFile, &frameIndex, bpf_high, bpf_low);
    // FIXME: read_leader reads one half_cycle too much; we need to account for this!
    printf("Leader done.\n");

    while (frameIndex < frames) {
        if (bits_done == 0) {
            for (uint16_t i = 0; i < start_bits; i++) {
                bool start_bit = get_bit(wavFile, cycles_per_bit, &frameIndex, bpf_high, bpf_low);
                if (start_bit) {
                    printf("Start bit was 1 at frame %lld, reached end of input.\n", frameIndex);
                    complete = true;
                    break;
                }
            }
            #ifdef KCS_DEBUG_CYCLES
            printf("<start bit done>\n");
            #endif
            //printf("BEGIN BYTE %llu @ %llu\n", byteIndex, frameIndex);
        }
        if (complete) break;

        bool bit = get_bit(wavFile, cycles_per_bit, &frameIndex, bpf_high, bpf_low);
        num += bit << bits_done;

        bits_done += 1;

        if (bits_done == data_bits) {
            buffer.data[byteIndex] = num;
            byteIndex += 1;

            // handle if buffer is full
            if (byteIndex >= buffer.size) {
                buffer.size += 1024;
                buffer.data = realloc(buffer.data, buffer.size);
            }

            #ifdef KCS_DEBUG_CYCLES
            printf("<parity+stop>\n");
            #endif

            // parity checks
            if (parity_mode == PARITY_ODD) {
                bool p = get_bit(wavFile, cycles_per_bit, &frameIndex, bpf_high, bpf_low);
                uint8_t bit_count = count_bits(num) + p;
                if (bit_count % 2 != 1)
                    parity_errors += 1;
            }
            else if (parity_mode == PARITY_EVEN) {
                bool p = get_bit(wavFile, cycles_per_bit, &frameIndex, bpf_high, bpf_low);
                uint8_t bit_count = count_bits(num) + p;
                if (bit_count % 2 != 0)
                    parity_errors += 1;
            }

            num = 0;
            bits_done = 0;

            for (uint8_t i = 0; i < stop_bits; i++) {
                bool stopBit = get_bit(wavFile, cycles_per_bit, &frameIndex, bpf_high, bpf_low);
                if (!stopBit) {
                    printf("ERROR: stop bit is 0 @ %d\n", frameIndex);
                }
            }

            #ifdef KCS_DEBUG_CYCLES
            printf("BYTE: %d\n", buffer.data[byteIndex - 1]);
            #endif
        }
    }
    // resize buffer to actual size
    buffer.size = byteIndex;

    DecodedKCS decoded = { parity_errors, buffer };

    return decoded;
}

const uint8_t CYCLE_SHAPE_LONG[18] = {155,193,217,232,242,249,252,255,160,100,62,38,23,13,6,3,0,95};
const uint8_t CYCLE_SHAPE_SHORT[9] = {157,220,245,255,151,85,43,17,0};

void write_cycle(WavFile *wavFile, uint64_t *frameIndex, bool value) {
    if (value) {
        for (int i = 0; i < 9; i++) {
            wavSetFrame(wavFile, *frameIndex, CYCLE_SHAPE_SHORT[i]);
            (*frameIndex)++;
        }
    } else {
        for (int i = 0; i < 18; i++) {
            wavSetFrame(wavFile, *frameIndex, CYCLE_SHAPE_LONG[i]);
            (*frameIndex)++;
        }
    }
}

void write_bit(WavFile *wavFile, uint16_t cycles_per_bit, uint64_t *frameIndex, bool value) {
    for (int i = 0; i < (value ? (cycles_per_bit * 2) : cycles_per_bit); i++) {
        write_cycle(wavFile, frameIndex, value);
    }
}

size_t KCS_estimate_wav_size(
    ByteBuffer data,
    uint32_t baud, uint8_t data_bits, uint8_t stop_bits,
    uint8_t start_bits, ParityMode parity_mode, uint16_t leader
) {
    size_t sampleRate = 22050; // default
    // assumes 8-bit mono wav file
    uint16_t cycles_per_bit = 1200 / baud;
    uint32_t samplesPerBit = (uint32_t)((double)sampleRate / BASE_FREQ * 2);
    uint32_t bitsPerByte = start_bits + data_bits + (parity_mode == PARITY_NONE ? 0 : 1) + stop_bits;

    size_t size = sampleRate * (uint32_t)leader * 2
        + data.size * bitsPerByte * cycles_per_bit * samplesPerBit;

    return size;
}

WavFile* KCS_encode(
    ByteBuffer data,
    uint32_t baud, uint8_t data_bits, uint8_t stop_bits,
    uint8_t start_bits, ParityMode parity_mode, uint16_t leader
) {
    uint16_t cycles_per_bit = 1200 / baud;
    WavFile* wavFile = wavNewDefault();
    wavAllocateData(wavFile, KCS_estimate_wav_size(data, baud, data_bits, stop_bits, start_bits, parity_mode, leader));

    uint64_t frameIndex = 0;

    // TODO: make this output work in the decoder!
    // leader
    if (leader > 0) {
        while (frameIndex < wavFile->fmt.sampleRate * (uint32_t)leader) {
            write_cycle(wavFile, &frameIndex, 1);
        }
    }

    for (size_t i = 0; i < data.size; i++) {
        // start bits
        for (uint16_t j = 0; j < start_bits; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, 0);
        }

        // actual data
        uint8_t num = data.data[i];

        for (uint16_t j = 0; j < data_bits; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, (num >> j) & 1);
        }

        // parity bits
        if (parity_mode == PARITY_ODD) {
            uint8_t bit_count = count_bits(num);
            write_bit(wavFile, cycles_per_bit, &frameIndex, bit_count % 2 == 0);
        }
        else if (parity_mode == PARITY_EVEN) {
            uint8_t bit_count = count_bits(num);
            write_bit(wavFile, cycles_per_bit, &frameIndex, bit_count % 2 == 1);
        }

        // stop bits
        for (uint16_t j = 0; j < stop_bits; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, 1);
        }
    }

    // "leader" but after
    if (leader > 0) {
        uint64_t begin = frameIndex;
        while (frameIndex - begin < wavFile->fmt.sampleRate * (uint32_t)leader) {
            write_cycle(wavFile, &frameIndex, 1);
        }
    }

    // trim wav file down to remove excess wave data

    return wavFile;
}

struct {
    uint32_t baud;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t start_bits;
    ParityMode parity_mode;
    uint16_t leader;
    bool makeWaveFile;
    bool consoleOutput;

} typedef KCS_Config;

int cmdDecode(KCS_Config config, char * *infile, char * *outfile) {
    FILE *file = fopen(*infile, "rb+");

    if (file == NULL) {
        printf("File not found.\n");
        return EXIT_FAILURE;
    }

    WavFile *wavFile = wavLoadFile(file);
    if (wavFile == NULL) {
        printf("Error occurred when trying to load WAV file, aborting.");
        return EXIT_FAILURE;
    }

    //printf("format: %.4s, %.4s\n", wavFile.chunkID, wavFile.format);
    //printf("ChunkSize: %d\n", wavFile.chunkSize);
    printf("Sample Rate: %dhz\n", wavFile->fmt.sampleRate);

    fclose(file);

    clock_t start = clock();

    DecodedKCS decodedKCS = KCS_decode(
        wavFile, config.baud, config.data_bits, config.stop_bits, 1, config.parity_mode, config.leader
    );

    clock_t end = clock();

    ByteBuffer buf = decodedKCS.data;

    //printf("Decoded Size: %d\n", buf.size);
    if (config.consoleOutput) {
        printf("%.*s\n", buf.size, buf.data);
    }
    else {
        // save to outfile
        FILE *f = fopen(*outfile, "wb");
        fwrite(buf.data, buf.size, 1, f);
        fclose(f);
    }

    printf("%lld bytes decoded", buf.size);
    if (config.parity_mode != PARITY_NONE) {
        printf(", %lld parity errors.", decodedKCS.parity_errors);
    }
    printf("\ndone in %.3fs\n", (double)(end - start) / CLOCKS_PER_SEC);

    wavFree(wavFile);
    free(buf.data);
        return EXIT_SUCCESS;
}

int cmdEncode(KCS_Config config, char * *infile, char * *outfile) {
    FILE *file = fopen(*infile, "rb+");

    if (file == NULL) {
        printf("File not found.\n");
        return EXIT_FAILURE;
    }

    ByteBuffer buf = {fbytesleft(file), NULL};
    buf.data = malloc(buf.size);
    fread(buf.data, 1, buf.size, file);

    clock_t start = clock();

    WavFile *wavFile = KCS_encode(
        buf, config.baud, config.data_bits, config.stop_bits,
        config.start_bits, config.parity_mode, config.leader
    );

    clock_t end = clock();

    // TODO: handle NULL wavFile

    FILE *f = fopen(*outfile, "wb");
    wavWriteFile(wavFile, f);
    fclose(f);

    printf("%lld bytes encoded", buf.size);
    printf("\ndone in %.3fs\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(buf.data);
    wavFree(wavFile);
    
    return EXIT_SUCCESS;
}

const char *info = "KCS Version 0.2 04-Apr-2023\n"
"Use: KCS [options] infile [outfile]\n\n"
"-Bn baud  1=600 2=1200   -O  odd parity\n"
"-Ln leader (sec)         -E  even parity\n"
"-C  console output       -D  7 data bits\n"
"-M  make wavefile        -S  1 stop bit\n"
"Default: decode WAV file, 300 baud, 8 data, 2 stop bits, no parity\n\n"
"Conversion utility for Kansas City Standard tapes.\n"
"Accepts 8/16-bit mono WAV wavefiles at 22,050 or 44,100 samples per second.\n"
"Written by Conqu3red.";

bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

int getOptions(int argc, char **argv, KCS_Config *config, char **infile, char **outfile) {
    for (int i = 1; i < argc; i++) {
        if (prefix("-C", argv[i])) config->consoleOutput = true;
        else if (prefix("-M", argv[i])) config->makeWaveFile = true;
        else if (prefix("-O", argv[i])) config->parity_mode = PARITY_ODD;
        else if (prefix("-E", argv[i])) config->parity_mode = PARITY_EVEN;
        else if (prefix("-D", argv[i])) config->data_bits = 7;
        else if (prefix("-S", argv[i])) config->stop_bits = 1;
        else if (prefix("-B", argv[i])) {
            int n = atoi(argv[i] + 2);
            if (n != 1 && n != 2) {
                printf("invalid option - specify -B1 or -B2 ... aborting\n");
                return EXIT_FAILURE;
            }

            config->baud = 600 * n;
        }
        else if (prefix("-L", argv[i])) {
            int n = atoi(argv[i] + 2);
            if (n < 0 || n >= (1 << 16)) {
                printf("invalid option ... aborting\n");
                return EXIT_FAILURE;
            }

            config->leader = n;
        }

        else {
            if (*infile == NULL) {
                *infile = malloc(strlen(argv[i]) + 1);
                strcpy(*infile, argv[i]);
            }
            else {
                *outfile = malloc(strlen(argv[i]) + 1);
                strcpy(*outfile, argv[i]);
            };
        }
    }

    return EXIT_SUCCESS;
}

int handleOptions(KCS_Config config, char **infile, char **outfile) {
    if (*outfile == NULL) {
        // TODO: generate outfile name based on infile and mode
        *outfile = malloc(strlen(*infile) + 3 + 1);
        strcpy(*outfile, *infile);
        if (config.makeWaveFile) {
            strcat(*outfile, ".wav");
        } else {
            strcat(*outfile, ".txt");
        }
    }

    printf("INFILE:  %s\n", *infile);
    printf("OUTFILE:  %s\n", *outfile);
    
    if (config.makeWaveFile) {
        // TODO: make wavefiles
        return cmdEncode(config, infile, outfile);
    }
    else {
        return cmdDecode(config, infile, outfile);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        printf("%s", info);
        return EXIT_SUCCESS;
    }

    KCS_Config config = {300, 8, 2, 1, PARITY_NONE, 1, false, false};
    
    char *infile = NULL;
    char *outfile = NULL;

    // TODO: these buffers are broken??

    int exit_code = getOptions(argc, argv, &config, &infile, &outfile);
    if (exit_code != EXIT_SUCCESS) {
        if (infile) free(infile);
        if (outfile) free(outfile);
        return exit_code;
    }

    if (infile == NULL) {
        if (outfile) free(outfile);
        printf("%s", info);
        return EXIT_SUCCESS;
    }

    exit_code = handleOptions(config, &infile, &outfile);
    
    if (infile) free(infile);
    if (outfile) free(outfile);

    return exit_code;
}
