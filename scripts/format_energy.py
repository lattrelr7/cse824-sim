import argparse
import re

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("energy", type=str, help="file with powertossimz output")
    args = parser.parse_args()

    out_file = open("out.txt", "wb")

    with open(args.energy, 'r') as energy:
        for line in energy:
            m = re.search('([\d\.]+) ([^\n.]+)\n', line)
            if(m is not None):
                print(m.group(2))
                new_line = m.group(2) + "\n"
                out_file.write(new_line.encode("utf-8"))

if __name__ == "__main__": main()
