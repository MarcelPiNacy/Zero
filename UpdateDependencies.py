import os
import pathlib
import json

wd = str(pathlib.Path().cwd())
file = open("dependencies.json")
d = json.load(file)
for name, url in d["dependencies"] :
    print("--- Updating " + name + " ---")
    if os.path.isdir(wd + "/dependencies/" + name + "/"):
        os.system("git -C " + wd + "/dependencies/" + name + " pull origin")
    else:
        os.system("git -C " + wd + "/dependencies/" + " clone " + url)
