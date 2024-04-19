import os

folder_path = "./"

for root, dirs, files in os.walk(folder_path):
    for file in files:
        if file.endswith(".h"):
            file_path = os.path.join(root, file)
            print(file_path)
            with open(file_path, 'r') as f:
                file_text = f.read()
                if "#pragma once" in file_text:
                    f_name = file.upper().replace(".", "_") + "__"
                    s1 = "#ifndef " + f_name
                    s2 = "#define " + f_name
                    s3 = "#endif //" + f_name
                    file_text = file_text.replace("#pragma once", s1 + "\n" + s2)
                    file_text += "\n" + s3
                    with open(file_path, 'w') as f:
                        f.write(file_text)