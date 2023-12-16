#!/usr/bin/python
# -*- coding: utf-8 -*-

# This script generates the version header to be used in REST installation
# J. Galan - Javier.Galan.Lacarra@cern.ch
# 8 - Oct - 2018

from __future__ import print_function

import os
import sys
import re
import filecmp
import subprocess

if len(sys.argv) >= 2:
    if sys.argv[1] == "--help":
        print("")
        print("Usage: ./generateVersionHeader.py XXX.h [SE]")
        print("")
        print(
            "If any additional argument different from OFF is provided as [SE].\nSchema evolution will be defined as enabled."
        )
        print("")
        exit(1)

print(
    "Each REST official release takes the name of a researcher that did a major contribution to sciences."
)
print(
    "We will search for particle physicists, physicists, mathematicians or any other discipline in that order."
)
print(
    "We will then use the birthdate to decide which physicist or scientist deserves the name of the release."
)
print("")
print(
    "You may use the following website: https://www.bornglorious.com/world/birthday/?pf=169470"
)
print("")
releaseName = input("Please, enter the name of the candidate!:  ")

print("Release name : " + str(releaseName))

outputHeader = "TRestVersion.h"
if len(sys.argv) >= 2:
    outputHeader = sys.argv[1]

seOption = "ON"
if len(sys.argv) > 2:
    seOption = sys.argv[2]

p = subprocess.Popen(
    ["git branch"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
)
(out, err) = p.communicate()

if len(str(err)) > 3:
    print(
        "WARNING! git repository is not initialized. TRestVersion.h will not be generated! REST will use default version header..."
    )
    exit(0)

branchName = (
    os.popen("git branch | grep -e \"^*\" | cut -d' ' -f 2").read().rstrip("\n")
)

# print(branchName)

commit = os.popen("git rev-parse --verify HEAD").read().rstrip("\n")

# print(commit)
# print(commit[0:8])

tag = os.popen("git describe --tags HEAD").read().rstrip("\n")
if tag.find("-") != -1:
    tag = tag[0 : tag.find("-")]

# command = "git log -1 --format=%ai " + str( tag )

datetime = os.popen("date").read().rstrip("\n")

date = datetime[0:10]
time = datetime[11:]

# print(date)
# print(time)

first = tag.find(".")
last = tag.rfind(".")

a = int(re.sub("[^0-9]", "", tag[0:first]))
b = int(re.sub("[^0-9]", "", tag[first + 1 : last]))
c = int(re.sub("[^0-9]", "", tag[last + 1 :]))

##### Increasing version!!

c = c + 1

restRelease = str(a) + "." + str(b) + "." + str(c)

code = a << 16 + b << 8 + c
codeA = a << 16
codeB = b << 8
codeC = c
code = codeA + codeB + codeC

print("-- Generating TRestVersion.h. Release : " + restRelease)

f = open("TRestVersion.tmp", "w")

f.write("#ifndef REST_Version\n")
f.write("#define REST_Version\n")

f.write(
    "/* This header has been generated using scripts/generateVersionHeader.py */\n\n"
)

f.write("/*\n")
f.write(" * The macros defined here can be used in the following way:\n")
f.write(" * \n")
f.write(" * #if REST_VERSION_CODE >= REST_VERSION(2,23,4)\n")
f.write(" *     #include <newheader.h>\n")
f.write(" * #else\n")
f.write(" *     #include <oldheader.h>\n")
f.write(" * #endif\n")
f.write(" *\n")
f.write(" */\n")
f.write('#define REST_RELEASE "' + restRelease + '"\n')
f.write('#define REST_RELEASE_DATE "' + date + '"\n')
f.write('#define REST_RELEASE_TIME "' + time + '"\n')
f.write('#define REST_RELEASE_NAME "' + releaseName + '"\n')
f.write('#define REST_GIT_COMMIT "' + commit[0:8] + '"\n')
f.write("#define REST_VERSION_CODE " + str(code) + "\n")
f.write("#define REST_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n")

if seOption == "OFF":
    f.write('#define REST_SCHEMA_EVOLUTION "OFF"\n')
else:
    f.write('#define REST_SCHEMA_EVOLUTION "ON"\n')

f.write("#endif\n")
f.close()

if os.path.exists(outputHeader) and filecmp.cmp("TRestVersion.tmp", outputHeader):
    os.remove("TRestVersion.tmp")
else:
    os.rename("TRestVersion.tmp", outputHeader)

print("\nA new version file has been generated at : " + outputHeader)
print(
    "To increase the version of REST use the generated file to replace the existing TRestVersion.h file.\n"
)
print("-----> mv " + outputHeader + " ../source/framework/core/inc/TRestVersion.h\n")
print("After replacing TRestVersion.h, add the file and commit it\n")
print("-----> git add ../source/framework/core/inc/TRestVersion.h")
print(
    '-----> git commit -m "Updated TRestVersion.h to v2.' + str(b) + "." + str(c) + '" '
)
print("\n")
print(
    "Once your PR has been accepted and merged, you should generate a new Git tag at the master branch.\n"
)
print(
    "-----> git tag -a v"
    + str(a)
    + "."
    + str(b)
    + "."
    + str(c)
    + ' -m "Update to version v'
    + str(a)
    + "."
    + str(b)
    + "."
    + str(c)
    + '"\n'
)
print(
    "And push the changes to repository. You should also push your branch to GitHub if you have not already.\n"
)
print("-----> git push origin v" + str(a) + "." + str(b) + "." + str(c) + "\n")
print(
    "IMPORTANT. Summarize the changes in the tag generated at the Gitlab repository website.\n"
)

exit(0)
