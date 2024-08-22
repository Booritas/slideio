
import sys
import os
import shutil

def process_python_dist(bin_path):
    if os.name == 'nt':
        code = os.system(f"{bin_path} -m pip install --user --upgrade pip")
        code = os.system(f"{bin_path} -m pip install --user --upgrade setuptools wheel")
        code = os.system(f"{bin_path} -m pip install six==1.14.0")
    code = os.system(f"{bin_path} -m pip install wheel")
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
    build_paths = []
    build_paths.append(os.path.abspath("./build"))
    # build_paths.append(os.path.abspath("../../build"))
    build_paths.append(os.path.abspath("../../build_py"))
    dist_path = os.path.abspath("./dist")
    if os.path.exists(dist_path):
        shutil.rmtree(dist_path)
    for line in lines:
        for build_path in build_paths:
            if os.path.exists(build_path):
                shutil.rmtree(build_path)
        line = line.rstrip("\n")
        print(f"============={line}==================")
        if(len(line)>1):
            process_python_dist(line)