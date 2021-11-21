import struct
import wave
import os
import enum
import time

long_cycle_vals = [155, 193, 217, 232, 242, 249, 252, 255, 160, 100, 62, 38, 23, 13, 6, 3, 0, 95]
short_cycle_vals = [157, 220, 245, 255, 151, 85, 43, 17, 0, 157, 220, 245, 255, 151, 85, 43, 17, 0]

long_cycle = struct.pack("<" + "B" * 18, *long_cycle_vals)
short_cycle = struct.pack("<" + "B" * 18, *short_cycle_vals)

cycle_length = 18

bit_count_lookup = bytes(bin(x).count("1") for x in range(256))

class ParityMode(enum.Enum):
    NONE = 0
    ODD = 1
    EVEN = 2

def decode_file(
    filename: str,
    baud = 300, data_bits = 8, stop_bits = 2,
    start_bits = 1, parity_mode: ParityMode = ParityMode.NONE, leader = 0
):

    cycles_per_bit = 1200 // baud

    frequency = 22050 # hz

    parity_errors = 0

    b = bytearray()

    with wave.open(filename, "rb") as f:
        frame_buffer = f.readframes(f.getnframes())

        frame_index = 0
        
        frames = f.getnframes() # leader is at the end as well
        
        bit_frame_length = cycles_per_bit * cycle_length

        sample_width = f.getsampwidth()

        bits_done = 0
        num = 0

        def get_bit(cycles_for_bit = cycles_per_bit) -> bool:
            nonlocal frame_index
            
            bit_length = cycles_for_bit * cycle_length

            cycles = 0
            top = False
            top_val = (256 ** sample_width) - 1

            local_index = 0

            #print("######")
            
            while local_index < bit_length:
                x = frame_buffer[frame_index + local_index]
                local_index += 1

                #print(x)
                
                if x == top_val: # top of cycle
                    top = True
                
                if not x and top:
                    cycles += 1
                    top = False
                
                
            frame_index += local_index
            return cycles == cycles_for_bit * 2
        
        # keep reading until we reach the initial start bit, then step back
        while True:
            bit = get_bit(1)
            if bit == 0:
                frame_index -= cycle_length
                break
            else:
                frames -= cycle_length
        

        while frame_index < frames:

            if bits_done == 0:
                for _ in range(start_bits):
                    get_bit()


            bit = get_bit()
            #print("BIT", bit)
            num += bit << bits_done
            
            bits_done += 1

            if bits_done == data_bits:
                b.append(num)

                if parity_mode == ParityMode.ODD:
                    p = get_bit()
                    bit_count = bit_count_lookup[num] + p
                    if bit_count % 2 != 1:
                        parity_errors += 1
                
                if parity_mode == ParityMode.EVEN:
                    p = get_bit()
                    bit_count = bit_count_lookup[num] + p
                    if bit_count % 2 != 0:
                        parity_errors += 1

                num = 0
                bits_done = 0

                for _ in range(stop_bits):
                    if frame_index < frames:
                        get_bit()
                    else:
                        print("stop bit clipped??")
    
    return parity_errors, b

def encode_file(
    data: bytes,
    filename: str,
    baud = 300, data_bits = 8, stop_bits = 2,
    start_bits = 1, parity_mode: ParityMode = ParityMode.NONE, leader = 0
):
    cycles_per_bit = 1200 // baud

    frequency = 22050 # hz

    with wave.open(filename, "wb") as f:
        parity_bits = parity_mode != ParityMode.NONE

        f.setnchannels(1)
        f.setsampwidth(1)
        f.setframerate(frequency)

        def push_cycle(long=False):
            if long:
                f.writeframesraw(long_cycle)
            else:
                f.writeframesraw(short_cycle)
        
        def push_bit(value: bool):
            for _ in range(cycles_per_bit):
                push_cycle(not value)
        

        for _ in range((leader * (frequency // cycle_length)) - cycles_per_bit):
            push_cycle(False)


        for byte in data:
            for _ in range(start_bits):
                push_bit(0)
            
            for i in range(data_bits):
                # little endian
                bit = (byte >> i) & 1

                push_bit(bit)
            
            if parity_mode == ParityMode.ODD:
                bit_count = bit_count_lookup[byte]
                push_bit(bit_count % 2 == 0)
            
            if parity_mode == ParityMode.EVEN:
                bit_count = bit_count_lookup[byte]
                push_bit(bit_count % 2 == 1)

            for _ in range(stop_bits):
                push_bit(1)
        
        # end leader
        for _ in range((leader * (frequency // cycle_length)) - cycles_per_bit):
            push_cycle(False)

def main():
    import argparse

    bauds = [300, 600, 1200]

    parser = argparse.ArgumentParser(
        description="Kansas City Standard Implementation",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("-B", type=int, choices=[0, 1, 2], default=0, help="Baud rate (0: 300, 1: 600, 2: 1200)", dest="baud")
    parser.add_argument("-L", type=int, default=0, help="Leader (seconds)", dest="leader")
    parser.add_argument("-M", action="store_true", help="Make Wavefile", dest="Make_Wavefile")
    parser.add_argument("-C", action="store_true", help="Console Output", dest="Console_Output")
    
    parser.add_argument("-O", action="store_const", const=ParityMode.ODD, default=ParityMode.NONE, dest="parity_mode", help="Odd Parity")
    parser.add_argument("-E", action="store_const", const=ParityMode.EVEN, default=ParityMode.NONE, dest="parity_mode", help="Even Parity")
    
    parser.add_argument("-S", action="store_const", const=1, default=2, dest="stop_bits", help="1 Stop Bit")

    parser.add_argument("-D", action="store_const", const=7, default=8, dest="data_bits", help="7 Data Bits")

    parser.add_argument("infile", type=str)
    parser.add_argument("outfile", nargs="?", type=str)

    args = parser.parse_args()
    args.baud = bauds[args.baud]

    if args.outfile is None:
        ext = ".wav" if args.Make_Wavefile else ".txt"
        args.outfile = os.path.splitext(args.infile)[0] + ext

    kwargs = vars(args).copy()
    #print(kwargs)
    del kwargs["infile"]
    del kwargs["outfile"]
    del kwargs["Make_Wavefile"]
    del kwargs["Console_Output"]
    
    print("INFILE: ", args.infile)
    
    if args.Make_Wavefile:
        if args.data_bits == 7:
            print("WARNING: The most significant bit in each byte will be lost!\n\t11111111 -> 01111111")
        
        start = time.perf_counter()
        with open(args.infile, "rb") as f:
            print("OUTFILE: ", args.outfile)
            encode_file(f.read(), args.outfile, **kwargs)
            
            end = time.perf_counter()
            print(f"done in {end - start :.2f}s")
    else:
        start = time.perf_counter()
        parity_errors, data = decode_file(args.infile, **kwargs)
        
        if args.Console_Output:
            print(data.decode("utf8"))
        
        else:
            print("OUTFILE: ", args.outfile)
            
            with open(args.outfile, "wb") as f:
                f.write(data)
        parity_message = f", {parity_errors} parity errors."
        
        print(f"{len(data)} bytes decoded{parity_message if args.parity_mode != ParityMode.NONE else ''}")
        end = time.perf_counter()
        print(f"done in {end - start :.2f}s")

if __name__ == "__main__":
    main()
