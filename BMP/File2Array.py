import os
import sys
from string import Template

template_file = Template('#define $filenameup_sz $size\nstatic const uint8_t $filename[$filenameup_sz] '
                         '= {\n$content,\n};')


def main(complete_filename: str):
    """
    converts file to array using some template
    :param complete_filename: name of source file (with path and extension)
    :return:
    """
    filename_main = os.path.splitext(complete_filename)[0]
    with open(complete_filename, 'rb') as source_file:
        with open(filename_main + '.h', 'w') as target_file:
            filename = os.path.split(filename_main)[1]
            filename_up_sz = filename.upper() + "_SZ"
            size = os.path.getsize(complete_filename)
            content = ','.join(['0x'+ '%02X' % x for x in source_file.read()])
            target_file.write(template_file.substitute({'filenameup_sz': filename_up_sz,
                                                        'size': str(size),
                                                        'filename': filename,
                                                        'content': content}))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("No file to convert")
    else:
        main(sys.argv[1])
