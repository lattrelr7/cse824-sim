import argparse
import random
import re

current_src_radio=1
standard_dbm = -60

cluster_nodes = []

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("top", type=str, help="file with topology")
    args = parser.parse_args()

    out_file = open("out.txt", "w")

    with open(args.top, 'r') as top:
        for line in top:
            m = re.search('\[(\d+)\]->(\d+)[^\n.]*', line)
            if(m is not None):
                print("type0", m.group(1), m.group(2))
                write_to_radios0(int(m.group(1)), int(m.group(2)), out_file)
            else:
                m = re.search('(\d+)->(\d+)[^\n.]*', line)
                if(m is not None):
                    print("type1", m.group(1), m.group(2))
                    write_to_radios1(int(m.group(1)), int(m.group(2)), out_file)
                    
    write_clusters(out_file)
    out_file.close()
    print("Wrote ", current_src_radio, " radios to out.")
                
def write_to_radios0(num, dst, out_file):
    global current_src_radio
    cluster = []
    for i in range(num):
        if(dst >= current_src_radio):
            print("WARNING: radio ", dst, " does not exist yet")
        out_file.write(str(current_src_radio) + " " + str(dst) + " " + str(standard_dbm) + "\n")
        out_file.write(str(dst) + " " + str(current_src_radio) + " " + str(standard_dbm) + "\n")
        cluster.append(current_src_radio)
        current_src_radio += 1
    cluster_nodes.append(cluster)

def write_to_radios1(src, dst, out_file):
    if(dst >= current_src_radio):
        print("WARNING: radio ", dst, " does not exist yet")
    out_file.write(str(src) + " " + str(dst) + " " + str(standard_dbm) + "\n")
    out_file.write(str(dst) + " " + str(src) + " " + str(standard_dbm) + "\n")

def write_clusters(out_file):
    for cluster in cluster_nodes:
        #Enable all nodes in cluster to hear each other
        for i in cluster:
            for j in cluster:
                if(i != j):
                    out_file.write(str(i) + " " + str(j) + " " + str(standard_dbm) + "\n")
            
if __name__ == "__main__": main()
