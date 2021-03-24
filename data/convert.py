import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", action="store_true",
                        help="convert to binary")
    parser.add_argument("filename", type=str,
                        help="the filename to convert")
    args = parser.parse_args()

    filename = args.filename
    is_binary = args.binary

    with open(filename, "rb") as f:
        data = f.read()

    for i in range(0, len(data), 2):
        byte2 = data[i]
        byte1 = 0 if i + 1 == len(data) else data[i + 1]

        if is_binary:
            print("%s%s" %\
                  (bin(byte1)[2:].zfill(8),
                   bin(byte2)[2:].zfill(8)))
        else:
            print("%s%s" %\
                  (hex(byte1)[2:].zfill(2),
                   hex(byte2)[2:].zfill(2)))

if __name__ == "__main__":
    main()
