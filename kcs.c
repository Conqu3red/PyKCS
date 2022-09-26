#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <time.h>

//#define KCS_DEBUG
//#define KCS_DEBUG_CYLES

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
        free(wavFile);
        if (wavFile->data.data != NULL) {
            free(wavFile->data.data);
        }
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
                printf("WAV Read Error: File says there are %lld bytes left but the file only has %lld bytes left.\n", size, bytes_left);
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

uint8_t wavGetFrame(WavFile *wavFile, size_t index) {
    // BUG: no support for multi-byte resolutions / multiple channels
    uint16_t bytesPerSample = wavFile->fmt.bitsPerSample / 8;
    if (index < wavFile->data.size) {
        if (bytesPerSample == 2) {
            //uint16_t v = *(uint16_t*)(wavFile->dataChunks[0].data + (index * 2));
            return wavFile->data.data[index * bytesPerSample + 1]; // most significant byte?
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
        wavFile->data.data[index * bytesPerSample + 1] = value; // most significant byte?
    }
    wavFile->data.data[index] = value;
}

uint64_t wavGetNumFrames(WavFile *wavFile) {
    // TODO: better solution, maybe like a FILE* ?
    return wavFile->data.size / (wavFile->fmt.bitsPerSample / 8);
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

static uint8_t getBitPrev = 0;
const uint32_t BASE_FREQ = 2400; // represents a 1

bool get_bit(WavFile *wavFile, uint8_t cyclesPerBit, uint64_t *frameIndex, bool peek) {
    uint32_t frame_window = (uint32_t)round((((double)wavFile->fmt.sampleRate)*cyclesPerBit*2)/(double)BASE_FREQ);

    uint32_t cycles = 0;
    uint64_t local_index = 0;
    uint32_t cLength = 0;

    const uint32_t threshold = (wavFile->fmt.sampleRate / 22050) * 2 * cyclesPerBit;

    while (local_index < frame_window + threshold) {
        if (((*frameIndex) + local_index) * (wavFile->fmt.bitsPerSample / 8) >= wavFile->data.allocSize) {
            if (!peek) {
                (*frameIndex) += local_index;
            }
            break; // end of data
        }
        
        uint8_t x = wavGetFrame(wavFile, (*frameIndex) + local_index);
        local_index++;
        cLength++;

        if ((getBitPrev >> 7) != (x >> 7) && local_index > 1) {
            #ifdef KCS_DEBUG_CYLES
            printf("%d ", cLength);
            #endif
            cLength = 0;
            cycles += 1;
        }

        getBitPrev = x;

        if (cycles == cyclesPerBit * 4) {
            break;
        } else if (local_index >= frame_window - threshold && cycles == cyclesPerBit * 2) {
            break;
        }
    }

    #ifdef KCS_DEBUG_CYLES
    printf("Cycles: %d, %d\n", cycles, local_index);
    #endif

    if (!peek) {
        (*frameIndex) += local_index;
    }
    return cycles >= cyclesPerBit * 3;
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

    uint64_t frames = wavGetNumFrames(wavFile);
    uint64_t frameIndex = 0;

    bool complete = false;

    while (get_bit(wavFile, 1, &frameIndex, true)) {
        get_bit(wavFile, 1, &frameIndex, false);
    }
    printf("Leader done.\n");

    while (frameIndex < frames) {
        if (bits_done == 0) {
            for (uint16_t i = 0; i < start_bits; i++) {
                bool start_bit = get_bit(wavFile, cycles_per_bit, &frameIndex, false);
                if (start_bit) {
                    printf("Start bit was 1, reached end of input.\n");
                    complete = true;
                    break;
                }
            }
            #ifdef KCS_DEBUG_CYLES
            printf("<start bit done>\n");
            #endif
        }
        if (complete) break;

        bool bit = get_bit(wavFile, cycles_per_bit, &frameIndex, false);
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

            #ifdef KCS_DEBUG_CYLES
            printf("<parity+stop>\n");
            #endif

            // parity checks
            if (parity_mode == PARITY_ODD) {
                bool p = get_bit(wavFile, cycles_per_bit, &frameIndex, false);
                uint8_t bit_count = count_bits(num) + p;
                if (bit_count % 2 != 1)
                    parity_errors += 1;
            }

            if (parity_mode == PARITY_EVEN) {
                bool p = get_bit(wavFile, cycles_per_bit, &frameIndex, false);
                uint8_t bit_count = count_bits(num) + p;
                if (bit_count % 2 != 0)
                    parity_errors += 1;
            }

            num = 0;
            bits_done = 0;

            for (uint8_t i = 0; i < stop_bits; i++) {
                bool stopBit = get_bit(wavFile, cycles_per_bit, &frameIndex, false);
                if (!stopBit) {
                    printf("ERROR: stop bit is 0\n");
                }
            }

            #ifdef KCS_DEBUG_CYLES
            printf("BYTE: %d\n", buffer.data[byteIndex - 1]);
            #endif
        }
    }
    // resize buffer to actual size
    buffer.size = byteIndex;

    DecodedKCS decoded = { parity_errors, buffer };

    return decoded;
}

void write_cycle(WavFile *wavFile, uint64_t *frameIndex, bool value, double intensity) {
    const uint16_t CENTER = 128;
    uint16_t AMPLITUDE = (uint16_t)(256.0*intensity);
    if (AMPLITUDE >= 256) AMPLITUDE = 255;

    uint32_t cycle_length;
    
    if (value) {
        // short cycle
        cycle_length = (uint32_t)((double)wavFile->fmt.sampleRate / BASE_FREQ);

    } else {
        // long cycle
        cycle_length = (uint32_t)((double)wavFile->fmt.sampleRate / BASE_FREQ * 2);
    }

    const uint32_t halfway = cycle_length / 2;
    for (int i = 0; i < cycle_length; i++) {
        wavSetFrame(wavFile, *frameIndex, i < halfway ? (CENTER+AMPLITUDE/2) : (CENTER-AMPLITUDE/2));
        (*frameIndex)++;
    }
}

void write_bit(WavFile *wavFile, uint16_t cycles_per_bit, uint64_t *frameIndex, bool value) {
    for (int i = 0; i < (value ? (cycles_per_bit * 2) : cycles_per_bit); i++) {
        write_cycle(wavFile, frameIndex, value, 1.0);
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
            write_cycle(wavFile, &frameIndex, 1, 0.5);
        }
    }

    for (size_t i = 0; i < data.size; i++) {
        // start bits
        for (uint16_t j = 0; j < start_bits; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, 0);
        }

        // actual data
        uint8_t num = data.data[i];

        for (uint16_t j = 0; j < 8; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, num & 1);
            num = num >> 1;
        }

        // TODO: parity etc

        // stop bits
        for (uint16_t j = 0; j < stop_bits; j++) {
            write_bit(wavFile, cycles_per_bit, &frameIndex, 1);
        }
    }

    // "leader" but after
    if (leader > 0) {
        uint64_t begin = frameIndex;
        while (frameIndex - begin < wavFile->fmt.sampleRate * (uint32_t)leader) {
            write_cycle(wavFile, &frameIndex, 1, 0.5);
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
    //printf("Sample Rate: %dhz\n", wavFile.fmt.sampleRate);

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

    printf("%d bytes decoded", buf.size);
    if (config.parity_mode != PARITY_NONE) {
        printf(", %d parity errors.", decodedKCS.parity_errors);
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

    printf("%d bytes encoded", buf.size);
    printf("\ndone in %.3fs\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(buf.data);
    wavFree(wavFile);
    
    return EXIT_SUCCESS;
}

const char *info = "KCS Version 0.1  21-Nov-2021\n"
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

int getOptions(int argc, const char *const argv[], KCS_Config *config, char * *infile, char * *outfile) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-C") == 0) config->consoleOutput = true;
        else if (strcmp(argv[i], "-M") == 0) config->makeWaveFile = true;
        else if (strcmp(argv[i], "-O") == 0) config->parity_mode = PARITY_ODD;
        else if (strcmp(argv[i], "-E") == 0) config->parity_mode = PARITY_EVEN;
        else if (strcmp(argv[i], "-D") == 0) config->data_bits = 7;
        else if (strcmp(argv[i], "-S") == 0) config->stop_bits = 1;
        else if (prefix("-B", argv[i])) {
            int n = atoi(argv[i] + 2);
            if (n != 1 && n != 2) {
                printf("invalid option ... aborting\n");
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

int handleOptions(KCS_Config config, char * *infile, char * *outfile) {
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

int main(int argc, const char *const argv[]) {
    if (argc == 1) {
        printf("%s", info);
        return EXIT_SUCCESS;
    }

    KCS_Config config = {300, 8, 2, 1, PARITY_NONE, 0, false, false};
    
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
