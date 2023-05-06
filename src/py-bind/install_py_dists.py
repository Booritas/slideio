import pathlib
import sys
import platform
import os
import zipfile
import argparse

def get_platform():
    #return 'Macos'
    platforms = {
        'linux' : 'Linux',
        'linux1' : 'Linux',
        'linux2' : 'Linux',
        'darwin' : 'Macos',
        'win32' : 'Windows'
    }
    if sys.platform not in platforms:
        return sys.platform
    return platforms[sys.platform]

def get_processor():
    return platform.processor()

def get_available_versions(folder_path):
    directories = []
    for dir_name in os.listdir(folder_path):
        if dir_name.startswith("3."):
            if os.path.isdir(os.path.join(folder_path, dir_name)):
                directories.append(dir_name)
    return directories

def find_file_by_name(file_name, folder_path):
    for root, dirs, files in os.walk(folder_path):
        if file_name in files:
            return os.path.join(root, file_name)
    return None

def process_macos(folder_path, available_versions):
    pythons = []
    for version in available_versions:
        version_parts = version.split('.')
        short_version = '.'.join(version_parts[0:2])
        file_name = 'python' + short_version
        version_folder = os.path.join(folder_path, version)
        print('Looking for ' + file_name + ' in ' + version_folder)
        python_path = find_file_by_name(file_name, version_folder)
        print('Found ' + python_path)
        pythons.append(python_path)
    return pythons

def save_files_to_text_file(files, output_file):
    with open(output_file, 'w') as f:
        for file_name in files:
            f.write(file_name + '\n')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Prepares a file with python distributives.')
    parser.add_argument('distributions_dir', type=str, help='Path of the directory to search for Python distributions (as zip files))')
    parser.add_argument('output_file', type=str, help='Path of the output file to save the list of Python executables to')
    args = parser.parse_args()

    target_folder =args.distributions_dir
    target_file = args.output_file
    
    root_path = pathlib.Path(__file__).parent.resolve()
    plt = get_platform()
    processor = get_processor()
    if plt == 'Macos':
        folder = os.path.join(root_path, 'MacOS', 'arm')
    else:
        raise Exception('Unsupported platform: ' + plt + ' ' + processor)
    available_versions = get_available_versions(target_folder)
    print('Available versions: ' + str(available_versions))
    pythons = process_macos(target_folder, available_versions)
    print('Found ' + str(len(pythons)) + ' python executables')
    print('Pythons:', pythons)
    save_files_to_text_file(pythons, target_file)
