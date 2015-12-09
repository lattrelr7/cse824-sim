import argparse
import re

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("energy", type=str, help="file with powertossimz output")
    args = parser.parse_args()

    total_bytes = 0
    hm_bytes = 0
    perc_hm = 0

    with open(args.energy, 'r') as energy:
        for line in energy:
            m = re.search('([\d\.]+) ([^\n.]+)\n', line)
            if(m is not None):
                #print(m.group(2))
                e_class = m.group(2).split(" ")[2].split(",")
                node = m.group(2).split(" ")[1].strip()
                if(len(e_class) > 3 and node == "(10):"):
                    if(e_class[2] == "SEND_MESSAGE" and e_class[3] == "ON"):
                        size = int(e_class[5].split(":")[1])
                        #print(size)
                        if(1):
                            total_bytes += size
                            if(size < 18):
                                hm_bytes += size
                            if(size > 18):
                                hm_bytes += size - 18
                        else:
                            size -= 13
                            total_bytes += size
                            if(size > 5):
                                hm_bytes += size - 5

    print("HM Bytes:",hm_bytes)
    print("Total Bytes:",total_bytes)
    print("%",(hm_bytes/total_bytes) * 100)

if __name__ == "__main__": main()
