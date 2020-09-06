import json

def get_bin_path(json_file_path):
    with open(json_file_path) as json_file:
        data = json.load(json_file)
        deps = data['dependencies'][0]
        paths = deps['bin_paths']
        path = paths[0]
        return path

if __name__ == "__main__":
    path = get_bin_path('conan/conanbuildinfo.json')
    print(path)