
import sys
import os

def process_python_dist(bin_path):
    code = os.system(f"{bin_path} setup.py sdist bdist_wheel")
    if code !=0:
        raise Exception(f"Error processing {bin_path}")

if __name__ == "__main__":
    arguments = sys.argv[1:]
    if len(arguments) < 1:
        raise Exception("Path to file with python distributions paths required")
    path_file = arguments[0]
    file = open(path_file, 'r')
    lines = file.readlines()
    for line in lines:
        line = line.rstrip("\n")
        print(f"============={line}==================")
        if(len(line)>1):
            process_python_dist(line)