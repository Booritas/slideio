import os

unique_lines = set()

def extract_lines(file_path):
    requires_found = False
    options_found = False

    with open(file_path, 'r') as file:
        for line in file:
            if line.strip() == '[requires]':
                requires_found = True
                continue
            elif line.strip() == '[options]':
                options_found = True
                break

            if requires_found and not options_found:
                unique_lines.add(line.strip())


def process_files(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('conanfile.txt'):
                file_path = os.path.join(root, file)
                extract_lines(file_path)

# Change the current directory to the desired directory
os.chdir('d:/Projects/slideio/slideio')

# Call the function to process the files
process_files('.')
print(f'------------------------------------')
for line in unique_lines:
    print(line)
